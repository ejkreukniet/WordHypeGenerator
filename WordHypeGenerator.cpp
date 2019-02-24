// WordHypeGenerator.cpp : Defines the entry point for the console application.
//

#include <fstream>
#include <iostream>
#include <algorithm>
#include <vector>
#include <iterator>
#include <dirent.h>

#include <time.h>
#include <string.h>
#include <sstream>

#define SYLLABLES_ON_SCREEN 6
#define MINIMUM_SYLLABLE_COUNT 2
#define EXTRA_SYLLABLE_COUNT 3
#define MINIMUM_WORD_COUNT 5

const int ALL_SYLLABLES_USED = (1 << SYLLABLES_ON_SCREEN) - 1;

struct syllable
{
	unsigned short index;
    std::string name;
	unsigned int count;
};

std::string g_language;

std::vector<syllable *> syllables;
std::vector<syllable *>::iterator it_syllables;

std::vector<std::vector <unsigned short>> words;
std::vector<std::vector <unsigned short>>::iterator it_words;

std::vector<unsigned short> unsort;

std::vector <unsigned short> checkSyllables(std::string word);

int generateGames();

unsigned long generatedGames = 0;

bool syllableComparator(syllable *i, syllable *j) 
{ 
	return (i->count == j->count ? (i->name.compare(j->name) < 0) : i->count >= j->count);
}

bool wordComparator(std::vector <unsigned short> i, std::vector <unsigned short> j)
{ 
	unsigned int a = 0;
	unsigned int b = 0;

	for (int x = 0; x < i.size(); ++x)
		a += i[x];

	for (int y = 0; y < j.size(); ++y)
		b += j[y];

	return (a < b);
}

std::string insertLanguage(const std::string filename)
{
    std::string result;
	
	result = filename;
	int p = result.find("%s");
	if (p)
	{
		result[p] = g_language[0];
        result[p+1] = g_language[1];
	}
	return result;
}

int main(int argc, char* argv[])
{
	g_language = argv[1];
	srand((unsigned)time(NULL));

    std::ifstream source(insertLanguage("WordHype_Parsed_%s.txt"), std::istream::in);

	printf("Reading words...\n");

	std::string buffer;
	while (!source.eof())
	{
	    std::getline(source, buffer);
        buffer.pop_back();
		words.push_back(checkSyllables(buffer));
	}

    source.close();

	printf("Sorting syllables...\n");

	// Sort syllables
	sort(syllables.begin(), syllables.end(), syllableComparator);

    printf("Writing syllables (%ld)...\n", syllables.size());

    std::ofstream count(insertLanguage("WordHype_Syllables_%s.csv"), std::istream::out);

	unsort.resize(syllables.size());
	
	unsigned int index = 0;
	for (it_syllables = syllables.begin(); it_syllables < syllables.end(); it_syllables++)
	{
		unsort[(*it_syllables)->index] = index++;

		count << (*it_syllables)->name;
		count << ";" << (*it_syllables)->count << std::endl;
	}
	count.close();

    printf("Reprocessing words...\n");

	// unsort words
	for (it_words = words.begin(); it_words < words.end(); it_words++)
	{
		unsigned int size = (*it_words).size();

		for (int w = 0; w < size; ++w)
		{
			(*it_words)[w] = unsort[(*it_words)[w]];
		}
	}

    printf("Sorting words...\n");

	// Sort words
	sort(words.begin(), words.end(), wordComparator);

    printf("Writing words (%ld)...\n", words.size());

    std::ofstream binary(insertLanguage("WordHype_Words_%s.bin"), std::istream::out | std::istream::binary);
    std::ofstream csv(insertLanguage("WordHype_Words_%s.csv"), std::istream::out);

	index = 0;
	unsigned short data = 0;
	for (it_words = words.begin(); it_words < words.end(); it_words++)
	{
		unsigned int size = (*it_words).size();

		csv << index << ";";

		binary.write((char *)&size, sizeof(size));
		for (int w = 0; w < size; ++w)
		{
			data = (*it_words)[w];

			csv << syllables[data]->name;

            binary.write((char *)&data, sizeof(data));
		}
		csv << std::endl;
		++index;
	}
	binary.close();
	csv.close();

    printf("Generating games...\n\n");

	generateGames();

	return 0;
}

std::vector <unsigned short> checkSyllables(std::string word)
{
    std::vector <unsigned short> result;

    std::istringstream check(word);
    std::string intermediate;

    while (getline(check, intermediate, '~'))
    {
		bool found = false;
		unsigned short index = 0;
		for (it_syllables = syllables.begin(); it_syllables < syllables.end(); it_syllables++)
		{
			if ((*it_syllables)->name == intermediate)
			{
				result.push_back(index);

				(*it_syllables)->count++;
				found = true;
				break;
			}
			++index;
		}

		if (!found)
		{
			result.push_back(index);

			auto s = new syllable();
			s->index = index;
			s->name = intermediate;
			s->count = 1;
			syllables.push_back(s);
		}
	}
	return result;
}

int generateGames()
{
    std::ofstream games(insertLanguage("WordHype_Games_%s.csv"), std::istream::out);

	unsigned int stack[SYLLABLES_ON_SCREEN];
	memset(&stack, 0, sizeof(stack));

	auto lastIndex = (unsigned int)syllables.size();

	std::vector<unsigned int> matched;

	while (generatedGames < 100)
	{
		bool ok;
		do
		{
			ok = true;

			stack[0] = rand() % lastIndex; // range 0 to RAND_MAX (32767)
			for (int level = 1; level < SYLLABLES_ON_SCREEN; ++level)
			{
				if (stack[level-1] == 0)
				{
					ok = false;
					break;
				}
				else
					stack[level] = rand() % stack[level-1];
			}
		}
		while (!ok);

		// check selected syllables with words
		matched.clear();
		int syllablesUsed = 0;
		int matchedMinimum = 0;
		int matchedSpecial = 0;

#pragma omp parallel default(none) shared(stack, matched, syllablesUsed, matchedMinimum, matchedSpecial, words)
{
#pragma omp for schedule(static) 
		for (int index = 0; index < words.size(); ++index) // Woordenlijst
		{
			unsigned int size = words[index].size(); // Aantal lettergrepen

            int syllablesInWord = 0;
            bool complete = true;
            for (int w = 0; complete && w < size; ++w) // Controleer alle lettergrepen van het woord
            {
                int sign = 1;
                bool found = false;
                for (int s = 0; s < SYLLABLES_ON_SCREEN; ++s) // Vergelijk met gekozen lettergrepen
                {
                    if (words[index][w] == stack[s])
                    {
                        syllablesInWord |= sign;
                        found = true;
                    }
                    sign <<= 1;
                }
                if (!found)
                    complete = false;
            }

            if (complete)
            {
                matched.push_back(index);
                if (size >= MINIMUM_SYLLABLE_COUNT)
                {
                    syllablesUsed |= syllablesInWord;
                    ++matchedMinimum;
                }
                if (size >= EXTRA_SYLLABLE_COUNT)
                    ++matchedSpecial;
            }
		}
}

		if (syllablesUsed == ALL_SYLLABLES_USED && matchedMinimum >= MINIMUM_WORD_COUNT && matchedSpecial)
		{
			// save the list
			for (int s = 0; s < SYLLABLES_ON_SCREEN; ++s)
			{
				if (s > 0)
					games << ",";
				games << syllables[stack[s]]->name;
			}
			games << ";";
			for (int m = 0; m < matched.size(); ++m)
			{
				if (m > 0)
					games << ",";
				for (int w = 0; w < words[matched[m]].size(); ++w)
				{
					if (w > 0)
						games << "~";
					games << syllables[words[matched[m]][w]]->name;
				}
			}
			games << std::endl;

			++generatedGames;

            printf("\rGames: %ld", generatedGames);
			for (int s = 0; s < SYLLABLES_ON_SCREEN; ++s)
			{
                printf("\t%d  ", stack[s]);
			}
		}
	}

	games.close();

	return 0;
}