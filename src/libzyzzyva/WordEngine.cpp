//---------------------------------------------------------------------------
// WordEngine.cpp
//
// A class to handle the loading and searching of words.
//
// Copyright 2004, 2005, 2006, 2007, 2008 Michael W Thelen <mthelen@gmail.com>.
//
// This file is part of Zyzzyva.
//
// Zyzzyva is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// Zyzzyva is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//---------------------------------------------------------------------------

#include "WordEngine.h"
#include "LetterBag.h"
#include "Auxil.h"
#include "Defs.h"
#include <QApplication>
#include <QFile>
#include <QRegExp>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

using namespace Defs;

//---------------------------------------------------------------------------
//  clearCache
//
//! Clear the word information cache for a lexicon.
//
//! @param lexicon the name of the lexicon
//---------------------------------------------------------------------------
void
WordEngine::clearCache(const QString& lexicon) const
{
    if (!lexiconData.contains(lexicon))
        return;

    lexiconData[lexicon]->wordCache.clear();
}

//---------------------------------------------------------------------------
//  connectToDatabase
//
//! Initialize the database connection for a lexicon.
//
//! @param lexicon the name of the lexicon
//! @param filename the name of the database file
//! @param errString returns the error string in case of error
//! @return true if successful, false otherwise
//---------------------------------------------------------------------------
bool
WordEngine::connectToDatabase(const QString& lexicon, const QString& filename,
                              QString* errString)
{
    Rand rng;
    rng.srand(QDateTime::currentDateTime().toTime_t(), Auxil::getPid());
    unsigned int r = rng.rand();
    QString dbConnectionName = "WordEngine_" + lexicon + "_" +
        QString::number(r);

    QSqlDatabase* db = new QSqlDatabase(
        QSqlDatabase::addDatabase("QSQLITE", dbConnectionName));
    db->setDatabaseName(filename);
    bool ok = db->open();

    if (!ok) {
        dbConnectionName = QString();
        if (errString)
            *errString = db->lastError().text();
        // delete db?
        return false;
    }

    LexiconData* data = lexiconData[lexicon];
    data->db = db;
    data->dbConnectionName = dbConnectionName;
    return true;
}

//---------------------------------------------------------------------------
//  disconnectFromDatabase
//
//! Remove the database connection for a lexicon.
//
//! @param lexicon the name of the lexicon
//! @return true if successful, false otherwise
//---------------------------------------------------------------------------
bool
WordEngine::disconnectFromDatabase(const QString& lexicon)
{
    if (!lexiconData.contains(lexicon))
        return true;

    QSqlDatabase* db = lexiconData[lexicon]->db;
    QString dbConnectionName = lexiconData[lexicon]->dbConnectionName;
    if (!db || !db->isOpen() || dbConnectionName.isEmpty())
        return true;

    delete db;
    lexiconData[lexicon]->db = 0;
    QSqlDatabase::removeDatabase(dbConnectionName);
    lexiconData[lexicon]->dbConnectionName.clear();
    return true;
}

//---------------------------------------------------------------------------
//  databaseIsConnected
//
//! Determine whether a lexicon database is connected.
//
//! @param lexicon the name of the lexicon
//! @return true if the database is connected, false otherwise
//---------------------------------------------------------------------------
bool
WordEngine::databaseIsConnected(const QString& lexicon) const
{
    return (lexiconData.contains(lexicon) && lexiconData[lexicon]->db);
}

//---------------------------------------------------------------------------
//  importTextFile
//
//! Import words from a text file.  The file is assumed to be in plain text
//! format, containing one word per line.
//
//! @param lexicon the name of the lexicon
//! @param filename the name of the file to import
//! @param loadDefinitions whether to load word definitions
//! @param errString returns the error string in case of error
//! @return the number of words imported
//---------------------------------------------------------------------------
int
WordEngine::importTextFile(const QString& lexicon, const QString& filename,
                           bool loadDefinitions, QString* errString)
{
    if (!lexiconData.contains(lexicon)) {
        lexiconData[lexicon] = new LexiconData;
        lexiconData[lexicon]->graph = new WordGraph;
    }

    WordGraph* graph = lexiconData[lexicon]->graph;

    QFile file (filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errString) {
            *errString = "Can't open file '" + filename + "': " +
                file.errorString();
        }
        return 0;
    }

    int imported = 0;
    char* buffer = new char[MAX_INPUT_LINE_LEN];
    while (file.readLine(buffer, MAX_INPUT_LINE_LEN) > 0) {
        QString line (buffer);
        line = line.simplified();
        if (!line.length() || (line.at(0) == '#'))
            continue;
        QString word = line.section(' ', 0, 0).toUpper();

        if (!graph->containsWord(word)) {
            QString alpha = Auxil::getAlphagram(word);
            ++lexiconData[lexicon]->numAnagramsMap[alpha];
        }

        graph->addWord(word);
        if (loadDefinitions) {
            QString definition = line.section(' ', 1);
            addDefinition(lexicon, word, definition);
        }
        ++imported;
    }

    delete[] buffer;
    return imported;
}

//---------------------------------------------------------------------------
//  importDawgFile
//
//! Import words from a DAWG file as generated by Graham Toal's dawgutils
//! programs: http://www.gtoal.com/wordgames/dawgutils/
//
//! @param lexicon the name of the lexicon
//! @param filename the name of the DAWG file to import
//! @param reverse whether the DAWG contains reversed words
//! @param errString returns the error string in case of error
//! @return true if successful, false otherwise
//---------------------------------------------------------------------------
bool
WordEngine::importDawgFile(const QString& lexicon, const QString& filename,
                           bool reverse, QString* errString, quint16*
                           expectedChecksum)
{
    if (!lexiconData.contains(lexicon)) {
        lexiconData[lexicon] = new LexiconData;
        lexiconData[lexicon]->graph = new WordGraph;
    }

    WordGraph* graph = lexiconData[lexicon]->graph;
    bool ok = graph->importDawgFile(filename, reverse, errString,
                                    expectedChecksum);
    return ok;
}

//---------------------------------------------------------------------------
//  importStems
//
//! Import stems for a lexicon from a file.  The file is assumed to be in
//! plain text format, containing one stem per line.  The file is also assumed
//! to contain stems of equal length.  All stems of different length than the
//! first stem will be discarded.
//
////@param lexicon the name of the lexicon
//! @param filename the name of the file to import
//! @param errString returns the error string in case of error
//! @return the number of stems imported
//---------------------------------------------------------------------------
int
WordEngine::importStems(const QString& lexicon, const QString& filename,
                        QString* errString)
{
    if (!lexiconData.contains(lexicon))
        return 0;

    QFile file (filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errString) {
            *errString = "Can't open file '" + filename + "': " +
                file.errorString();
        }
        return -1;
    }

    // XXX: At some point, may want to consider allowing words of varying
    // lengths to be in the same file?
    QStringList words;
    QSet<QString> alphagrams;
    int imported = 0;
    int length = 0;
    char* buffer = new char[MAX_INPUT_LINE_LEN];
    while (file.readLine(buffer, MAX_INPUT_LINE_LEN) > 0) {
        QString line (buffer);
        line = line.simplified();
        if (!line.length() || (line.at(0) == '#'))
            continue;
        QString word = line.section(' ', 0, 0);

        if (!length)
            length = word.length();

        if (length != int(word.length()))
            continue;

        words << word;
        alphagrams.insert(Auxil::getAlphagram(word));
        ++imported;
    }
    delete[] buffer;

    // Insert the stem list into the map, or append to an existing stem list
    LexiconData* data = lexiconData[lexicon];
    data->stems[length] += words;
    data->stemAlphagrams[length].unite(alphagrams);
    return imported;
}

//---------------------------------------------------------------------------
//  databaseSearch
//
//! Search the database for words matching the conditions in a search spec.
//! If a word list is provided, also ensure that result words are in that
//! list.
//
//! @param lexicon the name of the lexicon
//! @param optimizedSpec the search spec
//! @param wordList optional list of words that results must be in
//! @return a list of words matching the search spec
//---------------------------------------------------------------------------
QStringList
WordEngine::databaseSearch(const QString& lexicon, const SearchSpec&
                           optimizedSpec, const QStringList* wordList) const
{
    if (!lexiconData.contains(lexicon) || !lexiconData[lexicon]->db)
        return QStringList();

    // Build SQL query string
    QString queryStr = "SELECT word FROM words WHERE";
    bool foundCondition = false;
    QListIterator<SearchCondition> cit (optimizedSpec.conditions);
    while (cit.hasNext()) {
        SearchCondition condition = cit.next();
        if (getConditionPhase(condition) != DatabasePhase)
            continue;

        if (foundCondition)
            queryStr += " AND";
        foundCondition = true;

        switch (condition.type) {
            case SearchCondition::PatternMatch: {
                // XXX: eventually, account for negated condition
                QString str =
                    condition.stringValue.replace("?", "_").replace("*", "%");
                queryStr += " word LIKE '" + str + "'";
            }
            break;

            case SearchCondition::ProbabilityOrder: {
                // Lax boundaries
                if (condition.boolValue) {
                    queryStr += " max_probability_order>=" +
                        QString::number(condition.minValue) +
                        " AND min_probability_order<=" +
                        QString::number(condition.maxValue);
                }
                // Strict boundaries
                else {
                    queryStr += " probability_order";
                    if (condition.minValue == condition.maxValue) {
                        queryStr += "=" + QString::number(condition.minValue);
                    }
                    else {
                        queryStr += ">=" +
                            QString::number(condition.minValue) +
                            " AND probability_order<=" +
                            QString::number(condition.maxValue);
                    }
                }
            }
            break;

            case SearchCondition::Length:
            case SearchCondition::NumVowels:
            case SearchCondition::NumUniqueLetters:
            case SearchCondition::PointValue:
            case SearchCondition::NumAnagrams: {
                QString column;
                if (condition.type == SearchCondition::Length)
                    column = "length";
                if (condition.type == SearchCondition::NumVowels)
                    column = "num_vowels";
                if (condition.type == SearchCondition::NumUniqueLetters)
                    column = "num_unique_letters";
                if (condition.type == SearchCondition::PointValue)
                    column = "point_value";
                if (condition.type == SearchCondition::NumAnagrams)
                    column = "num_anagrams";

                queryStr += " " + column;
                if (condition.minValue == condition.maxValue) {
                    queryStr += "=" + QString::number(condition.minValue);
                }
                else {
                    queryStr += ">=" + QString::number(condition.minValue) +
                        " AND " + column + "<=" +
                        QString::number(condition.maxValue);
                }
            }
            break;

            case SearchCondition::IncludeLetters: {
                QString str = condition.stringValue;
                QMap<QChar, int> letters;
                for (int i = 0; i < str.length(); ++i) {
                    ++letters[str.at(i)];
                }

                QMapIterator<QChar, int> it (letters);
                for (int i = 0; it.hasNext(); ++i) {
                    it.next();
                    QChar c = it.key();
                    if (i)
                        queryStr += " AND";
                    queryStr += " word";
                    if (condition.negated)
                        queryStr += " NOT";
                    queryStr += " LIKE '%";
                    int count = condition.negated ? 1 : it.value();
                    for (int j = 0; j < count; ++j) {
                        queryStr += QString(c) + "%";
                    }
                    queryStr += "'";
                }
            }
            break;

            case SearchCondition::BelongToGroup: {
                SearchSet searchSet =
                    Auxil::stringToSearchSet(condition.stringValue);
                int target = condition.negated ? 0 : 1;
                switch (searchSet) {
                    case SetFrontHooks:
                    queryStr += " is_front_hook=" + QString::number(target);
                    break;

                    case SetBackHooks:
                    queryStr += " is_back_hook=" + QString::number(target);
                    break;

                    case SetHookWords:
                    if (condition.negated)
                        queryStr += " (is_front_hook=0 AND is_back_hook=0)";
                    else
                        queryStr += " (is_front_hook=1 OR is_back_hook=1)";
                    break;

                    default:
                    break;
                }
            }
            break;

            case SearchCondition::InWordList: {
                queryStr += " word";
                if (condition.negated)
                    queryStr += " NOT";
                queryStr += " IN (";
                QStringList words = condition.stringValue.split(QChar(' '));
                QStringListIterator it (words);
                bool firstWord = true;
                while (it.hasNext()) {
                    QString word = it.next();
                    if (!firstWord)
                        queryStr += ",";
                    firstWord = false;
                    queryStr += "'" + word + "'";
                }
                queryStr += ")";
            }
            break;

            default:
            break;
        }

    }

    // Make sure results are in the provided word list
    QMap<QString, QString> upperToLower;
    if (wordList) {
        queryStr += " AND word IN (";
        QStringListIterator it (*wordList);
        bool firstWord = true;
        while (it.hasNext()) {
            QString word = it.next();
            QString wordUpper = word.toUpper();
            upperToLower[wordUpper] = word;
            if (!firstWord)
                queryStr += ",";
            firstWord = false;
            queryStr += "'" + wordUpper + "'";
        }
        queryStr += ")";
    }

    // Query the database
    QStringList resultList;
    QSqlDatabase* db = lexiconData[lexicon]->db;
    QSqlQuery query (queryStr, *db);
    while (query.next()) {
        QString word = query.value(0).toString();
        if (!upperToLower.isEmpty() && upperToLower.contains(word)) {
            word = upperToLower[word];
        }
        resultList.append(word);
    }

    return resultList;
}

//---------------------------------------------------------------------------
//  applyPostConditions
//
//! Limit search results by search conditions that cannot be easily used to
//! limit word graph or database searches.
//
//! @param optimizedSpec the search spec
//! @param wordList optional list of words that results must be in
//! @return a list of words matching the search spec
//---------------------------------------------------------------------------
QStringList
WordEngine::applyPostConditions(const QString& lexicon,
    const SearchSpec& optimizedSpec, const QStringList& wordList) const
{
    QStringList returnList = wordList;

    // Check special postconditions
    QStringList::iterator wit;
    for (wit = returnList.begin(); wit != returnList.end();) {
        if (matchesPostConditions(lexicon, *wit, optimizedSpec.conditions))
            ++wit;
        else
            wit = returnList.erase(wit);
    }

    // Handle Limit by Probability Order conditions
    bool probLimitRangeCondition = false;
    bool legacyProbCondition = false;
    int probLimitRangeMin = 0;
    int probLimitRangeMax = 999999;
    int probLimitRangeMinLax = 0;
    int probLimitRangeMaxLax = 999999;
    QListIterator<SearchCondition> cit (optimizedSpec.conditions);
    while (cit.hasNext()) {
        SearchCondition condition = cit.next();
        if (condition.type == SearchCondition::LimitByProbabilityOrder) {
            probLimitRangeCondition = true;
            if (condition.boolValue) {
                if (condition.minValue > probLimitRangeMinLax)
                    probLimitRangeMinLax = condition.minValue;
                if (condition.maxValue < probLimitRangeMaxLax)
                    probLimitRangeMaxLax = condition.maxValue;
            }
            else {
                if (condition.minValue > probLimitRangeMin)
                    probLimitRangeMin = condition.minValue;
                if (condition.maxValue < probLimitRangeMax)
                    probLimitRangeMax = condition.maxValue;
            }
            if (condition.legacy)
                legacyProbCondition = true;
        }
    }

    // Keep only words in the probability order range
    if (probLimitRangeCondition) {
        if ((probLimitRangeMin > returnList.size()) ||
            (probLimitRangeMinLax > returnList.size()))
        {
            returnList.clear();
            return returnList;
        }

        // Convert from 1-based to 0-based offset
        --probLimitRangeMin;
        --probLimitRangeMax;
        --probLimitRangeMinLax;
        --probLimitRangeMaxLax;

        if (probLimitRangeMin < 0)
            probLimitRangeMin = 0;
        if (probLimitRangeMinLax < 0)
            probLimitRangeMinLax = 0;
        if (probLimitRangeMax > returnList.size() - 1)
            probLimitRangeMax = returnList.size() - 1;
        if (probLimitRangeMaxLax > returnList.size() - 1)
            probLimitRangeMaxLax = returnList.size() - 1;

        // Use the higher of the min values as working min
        int min = ((probLimitRangeMin > probLimitRangeMinLax)
                   ? probLimitRangeMin : probLimitRangeMinLax);

        // Use the lower of the max values as working max
        int max = ((probLimitRangeMax < probLimitRangeMaxLax)
                   ? probLimitRangeMax : probLimitRangeMaxLax);

        // Sort the words according to probability order
        LetterBag bag;
        QMap<QString, QString> probMap;

        foreach (QString word, returnList) {
            // FIXME: change this radix for new probability sorting - leave
            // alone for old probability sorting
            QString radix;
            QString wordUpper = word.toUpper();
            radix.sprintf("%09.0f",
                1e9 - 1 - bag.getNumCombinations(wordUpper));
            // Legacy probability order limits are sorted alphabetically, not
            // by alphagram
            if (!legacyProbCondition)
                radix += Auxil::getAlphagram(wordUpper);
            radix += wordUpper;
            probMap.insert(radix, word);
        }

        QStringList keys = probMap.keys();

        QString minRadix = keys[min];
        QString minCombinations = minRadix.left(9);
        while ((min > 0) && (min > probLimitRangeMin)) {
            if (minCombinations != keys[min - 1].left(9))
                break;
            --min;
        }

        QString maxRadix = keys[max];
        QString maxCombinations = maxRadix.left(9);
        while ((max < keys.size() - 1) && (max < probLimitRangeMax)) {
            if (maxCombinations != keys[max + 1].left(9))
                break;
            ++max;
        }

        returnList = probMap.values().mid(min, max - min + 1);
    }

    return returnList;
}

//---------------------------------------------------------------------------
//  lexiconIsLoaded
//
//! Determine whether a lexicon is loaded.
//
//! @param lexicon the name of the lexicon
//! @return true if the lexicon is loaded, false otherwise
//---------------------------------------------------------------------------
bool
WordEngine::lexiconIsLoaded(const QString& lexicon) const
{
    return lexiconData.contains(lexicon);
}

//---------------------------------------------------------------------------
//  isAcceptable
//
//! Determine whether a word is acceptable in a lexicon.
//
//! @param lexicon the name of the lexicon
//! @param word the word to look up
//! @return true if acceptable, false otherwise
//---------------------------------------------------------------------------
bool
WordEngine::isAcceptable(const QString& lexicon, const QString& word) const
{
    if (!lexiconData.contains(lexicon))
        return false;

    return lexiconData[lexicon]->graph->containsWord(word);
}

//---------------------------------------------------------------------------
//  search
//
//! Search for acceptable words matching a search specification.
//
//! @param lexicon the name of the lexicon
//! @param spec the search specification
//! @param allCaps whether to ensure the words in the list are all caps
//! @return a list of acceptable words
//---------------------------------------------------------------------------
QStringList
WordEngine::search(const QString& lexicon, const SearchSpec& spec, bool
                   allCaps) const
{
    if (!lexiconData.contains(lexicon))
        return QStringList();

    SearchSpec optimizedSpec = spec;
    optimizedSpec.optimize();

    // Discover which kinds of search conditions are present
    QMap<ConditionPhase, int> phaseCounts;
    int lengthConditions = 0;
    QListIterator<SearchCondition> cit (optimizedSpec.conditions);
    while (cit.hasNext()) {
        SearchCondition condition = cit.next();
        ConditionPhase phase = getConditionPhase(condition);
        ++phaseCounts[phase];
        if (condition.type == SearchCondition::Length)
            ++lengthConditions;
    }

    // Do not search the database based on Length conditions that were only
    // added by SearchSpec::optimize to optimize word graph searches
    if ((phaseCounts.contains(WordGraphPhase)) && lengthConditions &&
        (lengthConditions == phaseCounts.value(DatabasePhase)))
    {
        --phaseCounts[DatabasePhase];
    }

    // Search the word graph if necessary
    QStringList resultList;
    if (phaseCounts.contains(WordGraphPhase) ||
        !phaseCounts.contains(DatabasePhase))
    {
        resultList = wordGraphSearch(lexicon, optimizedSpec);
        if (resultList.isEmpty())
            return resultList;
    }

    // Search the database if necessary, passing word graph results
    if (phaseCounts.contains(DatabasePhase)) {
        resultList = databaseSearch(lexicon, optimizedSpec,
            phaseCounts.contains(WordGraphPhase) ? &resultList : 0);
        if (resultList.isEmpty())
            return resultList;
    }

    // Check post conditions if necessary
    if (phaseCounts.contains(PostConditionPhase)) {
        resultList = applyPostConditions(lexicon, optimizedSpec, resultList);
    }

    // Convert to all caps if necessary
    if (allCaps) {
        QStringList::iterator it;
        for (it = resultList.begin(); it != resultList.end(); ++it)
            *it = (*it).toUpper();
    }

    if (!resultList.isEmpty()) {
        clearCache(lexicon);
        addToCache(lexicon, resultList);
    }

    return resultList;
}

//---------------------------------------------------------------------------
//  wordGraphSearch
//
//! Search the word graph for words matching the conditions in a search spec.
//
//! @param lexicon the name of the lexicon
//! @param optimizedSpec the search spec
//! @return a list of words
//---------------------------------------------------------------------------
QStringList
WordEngine::wordGraphSearch(const QString& lexicon, const SearchSpec&
                            optimizedSpec) const
{
    if (!lexiconData.contains(lexicon))
        return QStringList();

    return lexiconData[lexicon]->graph->search(optimizedSpec);
}

//---------------------------------------------------------------------------
//  alphagrams
//
//! Transform a list of strings into a list of alphagrams of those strings.
//! The created list may be shorter than the original list.
//
//! @param list the list of strings
//! @return a list of alphagrams
//---------------------------------------------------------------------------
QStringList
WordEngine::alphagrams(const QStringList& strList) const
{
    // Insert into a set to remove duplicates
    QSet<QString> alphaSet;
    foreach (QString str, strList) {
        alphaSet.insert(Auxil::getAlphagram(str));
    }

    return alphaSet.toList();
}

//---------------------------------------------------------------------------
//  getWordInfo
//
//! Get information about a word from the database.  Also cache the
//! information for future queries.  Fail if the information is not in the
//! cache and the database is not open.
//
//! @param lexicon the name of the lexicon
//! @param word the word
//! @return information about the word from the database
//---------------------------------------------------------------------------
WordEngine::WordInfo
WordEngine::getWordInfo(const QString& lexicon, const QString& word) const
{
    if (word.isEmpty())
        return WordInfo();

    if (!lexiconData.contains(lexicon))
        return WordInfo();

    if (lexiconData[lexicon]->wordCache.contains(word)) {
        //qDebug("Cache HIT: |%s|", word.toUtf8().data());
        return lexiconData[lexicon]->wordCache[word];
    }
    //qDebug("Cache MISS: |%s|", word.toUtf8().data());

    WordInfo info;
    QSqlDatabase* db = lexiconData[lexicon]->db;
    if (!db || !db->isOpen())
        return info;

    QString qstr = "SELECT probability_order, min_probability_order, "
        "max_probability_order, num_vowels, num_unique_letters, num_anagrams, "
        "point_value, front_hooks, back_hooks, is_front_hook, is_back_hook, "
        "lexicon_symbols, definition FROM words WHERE word=?";
    QSqlQuery query (*db);
    query.prepare(qstr);
    query.bindValue(0, word);
    query.exec();

    if (query.next()) {
        info.word = word;
        info.probabilityOrder    = query.value(0).toInt();
        info.minProbabilityOrder = query.value(1).toInt();
        info.maxProbabilityOrder = query.value(2).toInt();
        info.numVowels           = query.value(3).toInt();
        info.numUniqueLetters    = query.value(4).toInt();
        info.numAnagrams         = query.value(5).toInt();
        info.pointValue          = query.value(6).toInt();
        info.frontHooks          = query.value(7).toString();
        info.backHooks           = query.value(8).toString();
        info.isFrontHook         = query.value(9).toBool();
        info.isBackHook          = query.value(10).toBool();
        info.lexiconSymbols      = query.value(11).toString();
        info.definition          = query.value(12).toString();
        lexiconData[lexicon]->wordCache[word] = info;
    }

    return info;
}

//---------------------------------------------------------------------------
//  getNumWords
//
//! Return a word count for the current lexicon.
//
//! @param lexicon the name of the lexicon
//! @return the word count
//---------------------------------------------------------------------------
int
WordEngine::getNumWords(const QString& lexicon) const
{
    if (!lexiconData.contains(lexicon))
        return 0;

    QSqlDatabase* db = lexiconData[lexicon]->db;
    if (db && db->isOpen()) {
        QString qstr = "SELECT count(*) FROM words";
        QSqlQuery query (qstr, *db);
        if (query.next())
            return query.value(0).toInt();
    }
    else
        return lexiconData[lexicon]->graph->getNumWords();

    return 0;
}

//---------------------------------------------------------------------------
//  getDefinition
//
//! Return the definition associated with a word.
//
//! @param lexicon the name of the lexicon
//! @param word the word whose definition to look up
//! @param replaceLinks whether to resolve links to other definitions
//! @return the definition, or empty String if no definition
//---------------------------------------------------------------------------
QString
WordEngine::getDefinition(const QString& lexicon, const QString& word,
                          bool replaceLinks) const
{
    if (!lexiconData.contains(lexicon))
        return QString();

    QString definition;

    WordInfo info = getWordInfo(lexicon, word);
    if (info.isValid()) {
        if (replaceLinks) {
            QStringList defs = info.definition.split(" / ");
            definition = QString();
            foreach (QString def, defs) {
                if (!definition.isEmpty())
                    definition += "\n";
                definition += def;
            }
            return definition;
        }
        else {
            return info.definition;
        }
    }

    else {
        if (!lexiconData[lexicon]->definitions.contains(word))
            return QString();

        const QMultiMap<QString, QString>& mmap =
            lexiconData[lexicon]->definitions.value(word);
        QMapIterator<QString, QString> it (mmap);
        while (it.hasNext()) {
            it.next();
            if (!definition.isEmpty()) {
                if (replaceLinks)
                    definition += "\n";
                else
                    definition += " / ";
            }
            definition += it.value();
        }
        return definition;
    }
}

//---------------------------------------------------------------------------
//  getFrontHookLetters
//
//! Get a string of letters that can be added to the front of a word to make
//! other valid words.
//
//! @param lexicon the name of the lexicon
//! @param word the word, assumed to be upper case
//! @return a string containing lower case letters representing front hooks
//---------------------------------------------------------------------------
QString
WordEngine::getFrontHookLetters(const QString& lexicon, const QString& word)
    const
{
    QString ret;

    WordInfo info = getWordInfo(lexicon, word);
    if (info.isValid()) {
        ret = info.frontHooks;
    }

    else {
        SearchSpec spec;
        SearchCondition condition;
        condition.type = SearchCondition::PatternMatch;
        condition.stringValue = "?" + word;
        spec.conditions.append(condition);

        // Get and sort first letters of each word
        QStringList words = search(lexicon, spec, true);
        QList<QChar> letters;
        foreach (QString str, words) {
            letters.append(str.at(0).toLower());
        }
        qSort(letters);

        QListIterator<QChar> it (letters);
        while (it.hasNext())
            ret += it.next();
    }

    return ret;
}

//---------------------------------------------------------------------------
//  getBackHookLetters
//
//! Get a string of letters that can be added to the back of a word to make
//! other valid words.
//
//! @param lexicon the name of the lexicon
//! @param word the word, assumed to be upper case
//! @return a string containing lower case letters representing back hooks
//---------------------------------------------------------------------------
QString
WordEngine::getBackHookLetters(const QString& lexicon, const QString& word) const
{
    QString ret;

    WordInfo info = getWordInfo(lexicon, word);
    if (info.isValid()) {
        ret = info.backHooks;
    }

    else {
        SearchSpec spec;
        SearchCondition condition;
        condition.type = SearchCondition::PatternMatch;
        condition.stringValue = word + "?";
        spec.conditions.append(condition);

        // Get and sort last letters of each word
        QStringList words = search(lexicon, spec, true);
        QList<QChar> letters;
        foreach (QString str, words) {
            letters.append(str.at(str.length() - 1).toLower());
        }
        qSort(letters);

        QListIterator<QChar> it (letters);
        while (it.hasNext())
            ret += it.next();
    }

    return ret;
}

//---------------------------------------------------------------------------
//  addToCache
//
//! Add information about a list of words to the cache.
//
//! @param lexicon the name of the lexicon
//! @param words the list of words
//---------------------------------------------------------------------------
void
WordEngine::addToCache(const QString& lexicon, const QStringList& words) const
{
    if (words.isEmpty() || !lexiconData.contains(lexicon))
        return;

    QSqlDatabase* db = lexiconData[lexicon]->db;
    if (!db || !db->isOpen())
        return;

    QString qstr = "SELECT word, probability_order, min_probability_order, "
        "max_probability_order, num_vowels, num_unique_letters, num_anagrams, "
        "point_value, front_hooks, back_hooks, is_front_hook, is_back_hook, "
        "lexicon_symbols, definition FROM words WHERE word IN (";

    QStringListIterator it (words);
    for (int i = 0; it.hasNext(); ++i) {
        if (i)
            qstr += ", ";
        qstr += "'" + it.next() + "'";
    }
    qstr += ")";

    QSqlQuery query (*db);
    query.prepare(qstr);
    query.exec();

    while (query.next()) {
        WordInfo info;
        info.word                = query.value(0).toString();
        info.probabilityOrder    = query.value(1).toInt();
        info.minProbabilityOrder = query.value(2).toInt();
        info.maxProbabilityOrder = query.value(3).toInt();
        info.numVowels           = query.value(4).toInt();
        info.numUniqueLetters    = query.value(5).toInt();
        info.numAnagrams         = query.value(6).toInt();
        info.pointValue          = query.value(7).toInt();
        info.frontHooks          = query.value(8).toString();
        info.backHooks           = query.value(9).toString();
        info.isFrontHook         = query.value(10).toBool();
        info.isBackHook          = query.value(11).toBool();
        info.lexiconSymbols      = query.value(12).toString();
        info.definition          = query.value(13).toString();
        lexiconData[lexicon]->wordCache[info.word] = info;
    }
}

//---------------------------------------------------------------------------
//  matchesPostConditions
//
//! Test whether a word matches certain conditions.  Not all conditions in the
//! list are tested.  Only the conditions that cannot be easily tested in
//! WordGraph::search are tested here.
//
//! @param lexicon the name of the lexicon
//! @param word the word to be tested
//! @param conditions the list of conditions to test
//! @return true if the word matches all special conditions, false otherwise
//---------------------------------------------------------------------------
bool
WordEngine::matchesPostConditions(const QString& lexicon, const QString& word,
                                  const QList<SearchCondition>& conditions) const
{
    if (!lexiconData.contains(lexicon))
        return false;

    QString wordUpper = word.toUpper();
    QListIterator<SearchCondition> it (conditions);
    while (it.hasNext()) {
        const SearchCondition& condition = it.next();
        if (getConditionPhase(condition) != PostConditionPhase)
            continue;

        switch (condition.type) {

            case SearchCondition::Prefix:
            if ((!isAcceptable(lexicon, condition.stringValue + wordUpper))
                ^ condition.negated)
                return false;
            break;

            case SearchCondition::Suffix:
            if ((!isAcceptable(lexicon, wordUpper + condition.stringValue))
                ^ condition.negated)
                return false;
            break;

            case SearchCondition::BelongToGroup: {
                SearchSet searchSet =
                    Auxil::stringToSearchSet(condition.stringValue);
                if (searchSet == UnknownSearchSet)
                    continue;
                if (!isSetMember(lexicon, wordUpper, searchSet)
                    ^ condition.negated)
                    return false;
            }
            break;

            default: break;
        }
    }

    return true;
}

//---------------------------------------------------------------------------
//  isSetMember
//
//! Determine whether a word is a member of a set.  Assumes the word has
//! already been determined to be acceptable.
//
//! @param lexicon the name of the lexicon
//! @param word the word to look up
//! @param ss the search set
//! @return true if a member of the set, false otherwise
//---------------------------------------------------------------------------
bool
WordEngine::isSetMember(const QString& lexicon, const QString& word,
                        SearchSet ss) const
{
    if (!lexiconData.contains(lexicon))
        return false;

    static QString typeTwoChars = "AAADEEEEGIIILNNOORRSSTTU";
    static int typeTwoCharsLen = typeTwoChars.length();
    static LetterBag letterBag("A:9 B:2 C:2 D:4 E:12 F:2 G:3 H:2 I:9 J:1 "
                               "K:1 L:4 M:2 N:6 O:8 P:2 Q:1 R:6 S:4 T:6 "
                               "U:4 V:2 W:2 X:1 Y:2 Z:1 _:2");
    static double typeThreeSevenCombos
        = letterBag.getNumCombinations("HUNTERS");
    static double typeThreeEightCombos
        = letterBag.getNumCombinations("NOTIFIED");

    switch (ss) {
        case SetHookWords:
        return (isAcceptable(lexicon, word.left(word.length() - 1)) ||
                isAcceptable(lexicon, word.right(word.length() - 1)));

        case SetFrontHooks:
        return isAcceptable(lexicon, word.right(word.length() - 1));

        case SetBackHooks:
        return isAcceptable(lexicon, word.left(word.length() - 1));

        case SetHighFives: {
            if (word.length() != 5)
                return false;

            bool ok = false;
            for (int i = 0; i < word.length(); ++i) {
                int value = letterBag.getLetterValue(word[i]);
                if (value > 5)
                    return false;
                if (((value == 4) || (value == 5)) && ((i == 0) || (i == 4)))
                    ok = true;
            }
            return ok;
        }

        case SetTypeOneSevens: {
            if (word.length() != 7)
                return false;

            if (!lexiconData[lexicon]->stemAlphagrams.contains(word.length() - 1))
                return false;

            QString agram = Auxil::getAlphagram(word);
            const QSet<QString>& alphaSet =
                lexiconData[lexicon]->stemAlphagrams[word.length() - 1];

            for (int i = 0; i < int(agram.length()); ++i) {
                if (alphaSet.contains(agram.left(i) +
                                      agram.right(agram.length() - i - 1)))
                {
                    return true;
                }
            }
            return false;
        }

        case SetTypeOneEights: {
            if (word.length() != 8)
                return false;

            if (!lexiconData[lexicon]->stemAlphagrams.contains(word.length() - 2))
                return false;

            // Compare the letters of the word with the letters of each
            // alphagram, ensuring that no more than two letters in the word
            // are missing from the alphagram.
            QString agram = Auxil::getAlphagram(word);
            const QSet<QString>& alphaSet =
                lexiconData[lexicon]->stemAlphagrams[word.length() - 2];

            QSetIterator<QString> it (alphaSet);
            while (it.hasNext()) {
                QString setAlphagram = it.next();
                int missing = 0;
                int saIndex = 0;
                for (int i = 0; (i < int(agram.length())) &&
                                (saIndex < setAlphagram.length()); ++i)
                {
                    if (agram.at(i) == setAlphagram.at(saIndex))
                        ++saIndex;
                    else
                        ++missing;
                    if (missing > 2)
                        break;
                }
                if (missing <= 2)
                    return true;
            }
            return false;
        }

        case SetTypeTwoSevens:
        case SetTypeTwoEights:
        {
            if (((ss == SetTypeTwoSevens) && (word.length() != 7)) ||
                ((ss == SetTypeTwoEights) && (word.length() != 8)))
                return false;

            bool ok = false;
            QString alphagram = Auxil::getAlphagram(word);
            int wi = 0;
            QChar wc = alphagram[wi];
            for (int ti = 0; ti < typeTwoCharsLen; ++ti) {
                QChar tc = typeTwoChars[ti];
                if (tc == wc) {
                    ++wi;
                    if (wi == alphagram.length()) {
                        ok = true;
                        break;
                    }
                    wc = alphagram[wi];
                }
            }
            return (ok && !isSetMember(lexicon, word,
                (ss == SetTypeTwoSevens ? SetTypeOneSevens : SetTypeOneEights)));
        }

        case SetTypeThreeSevens: {
            if (word.length() != 7)
                return false;

            double combos = letterBag.getNumCombinations(word);
            return ((combos >= typeThreeSevenCombos) &&
                    !isSetMember(lexicon, word, SetTypeOneSevens) &&
                    !isSetMember(lexicon, word, SetTypeTwoSevens));
        }

        case SetTypeThreeEights: {
            if (word.length() != 8)
                return false;

            double combos = letterBag.getNumCombinations(word);
            return ((combos >= typeThreeEightCombos) &&
                    !isSetMember(lexicon, word, SetTypeOneEights) &&
                    !isSetMember(lexicon, word, SetTypeTwoEights));
        }

        case SetEightsFromSevenLetterStems: {
            if (word.length() != 8)
                return false;

            if (!lexiconData[lexicon]->stemAlphagrams.contains(word.length() - 1))
                return false;

            QString agram = Auxil::getAlphagram(word);
            const QSet<QString>& alphaSet =
                lexiconData[lexicon]->stemAlphagrams[word.length() - 1];

            for (int i = 0; i < int(agram.length()); ++i) {
                if (alphaSet.contains(agram.left(i) +
                                      agram.right(agram.length() - i - 1)))
                {
                    return true;
                }
            }
            return false;
        }

        default: return false;
    }
}

//---------------------------------------------------------------------------
//  getNumAnagrams
//
//! Determine the number of valid anagrams of a word.
//
//! @param lexicon the name of the lexicon
//! @param word the word
//! @return the number of valid anagrams
//---------------------------------------------------------------------------
int
WordEngine::getNumAnagrams(const QString& lexicon, const QString& word) const
{
    if (!lexiconData.contains(lexicon))
        return 0;

    WordInfo info = getWordInfo(lexicon, word);
    if (info.isValid()) {
        return info.numAnagrams;
    }

    else {
        QString alpha = Auxil::getAlphagram(word);
        return lexiconData[lexicon]->numAnagramsMap.value(alpha);
    }
}

//---------------------------------------------------------------------------
//  getProbabilityOrder
//
//! Get the probability order for a word.
//
//! @param lexicon the name of the lexicon
//! @param word the word
//! @return the probability order
//---------------------------------------------------------------------------
int
WordEngine::getProbabilityOrder(const QString& lexicon, const QString& word)
    const
{
    if (!lexiconData.contains(lexicon))
        return 0;

    WordInfo info = getWordInfo(lexicon, word);
    return info.isValid() ? info.probabilityOrder : 0;
}

//---------------------------------------------------------------------------
//  getMinProbabilityOrder
//
//! Get the minimum probability order for a word.
//
//! @param lexicon the name of the lexicon
//! @param word the word
//! @return the probability order
//---------------------------------------------------------------------------
int
WordEngine::getMinProbabilityOrder(const QString& lexicon, const QString&
                                   word) const
{
    if (!lexiconData.contains(lexicon))
        return 0;

    WordInfo info = getWordInfo(lexicon, word);
    return info.isValid() ? info.minProbabilityOrder : 0;
}

//---------------------------------------------------------------------------
//  getMaxProbabilityOrder
//
//! Get the maximum probability order for a word.
//
//! @param lexicon the name of the lexicon
//! @param word the word
//! @return the probability order
//---------------------------------------------------------------------------
int
WordEngine::getMaxProbabilityOrder(const QString& lexicon, const QString&
                                   word) const
{
    if (!lexiconData.contains(lexicon))
        return 0;

    WordInfo info = getWordInfo(lexicon, word);
    return info.isValid() ? info.maxProbabilityOrder : 0;
}

//---------------------------------------------------------------------------
//  getNumVowels
//
//! Get the number of vowels in a word.
//
//! @param lexicon the name of the lexicon
//! @param word the word
//! @return the number of vowels
//---------------------------------------------------------------------------
int
WordEngine::getNumVowels(const QString& lexicon, const QString& word) const
{
    // No test of lexiconData because we want to calculate if even not cached
    WordInfo info = getWordInfo(lexicon, word);
    return info.isValid() ? info.numVowels : Auxil::getNumVowels(word);
}

//---------------------------------------------------------------------------
//  getNumUniqueLetters
//
//! Get the number of unique letters in a word.
//
//! @param lexicon the name of the lexicon
//! @param word the word
//! @return the number of unique letters
//---------------------------------------------------------------------------
int
WordEngine::getNumUniqueLetters(const QString& lexicon, const QString& word) const
{
    // No test of lexiconData because we want to calculate if even not cached
    WordInfo info = getWordInfo(lexicon, word);
    return info.isValid() ? info.numUniqueLetters
                          : Auxil::getNumUniqueLetters(word);
}

//---------------------------------------------------------------------------
//  getPointValue
//
//! Get the point value for a word.
//
//! @param lexicon the name of the lexicon
//! @param word the word
//! @return the point value
//---------------------------------------------------------------------------
int
WordEngine::getPointValue(const QString& lexicon, const QString& word) const
{
    if (!lexiconData.contains(lexicon))
        return 0;

    WordInfo info = getWordInfo(lexicon, word);
    return info.isValid() ? info.pointValue : 0;
}

//---------------------------------------------------------------------------
//  getIsFrontHook
//
//! Determine whether a word is a front hook.
//
//! @param lexicon the name of the lexicon
//! @param word the word
//! @return true if the word is a front hook, false otherwise
//---------------------------------------------------------------------------
bool
WordEngine::getIsFrontHook(const QString& lexicon, const QString& word) const
{
    if (!lexiconData.contains(lexicon))
        return 0;

    WordInfo info = getWordInfo(lexicon, word);
    return info.isValid() ? info.isFrontHook : false;
}

//---------------------------------------------------------------------------
//  getIsBackHook
//
//! Determine whether a word is a back hook.
//
//! @param lexicon the name of the lexicon
//! @param word the word
//! @return true if the word is a back hook, false otherwise
//---------------------------------------------------------------------------
bool
WordEngine::getIsBackHook(const QString& lexicon, const QString& word) const
{
    if (!lexiconData.contains(lexicon))
        return 0;

    WordInfo info = getWordInfo(lexicon, word);
    return info.isValid() ? info.isBackHook : false;
}

//---------------------------------------------------------------------------
//  getLexiconSymbols
//
//! Get lexicon symbols to be displayed along with this word.
//
//! @param lexicon the name of the lexicon
//! @param word the word
//! @return a lexicon symbol string
//---------------------------------------------------------------------------
QString
WordEngine::getLexiconSymbols(const QString& lexicon, const QString& word) const
{
    if (!lexiconData.contains(lexicon))
        return 0;

    WordInfo info = getWordInfo(lexicon, word);
    return info.isValid() ? info.lexiconSymbols : QString();
}

//---------------------------------------------------------------------------
//  nonGraphSearch
//
//! Search for valid words matching conditions that can be matched without
//! searching the word graph.
//
//! @param lexicon the name of the lexicon
//! @param spec the search specification
//! @return a list of acceptable words matching the In Word List conditions
//---------------------------------------------------------------------------
QStringList
WordEngine::nonGraphSearch(const QString& lexicon, const SearchSpec& spec) const
{
    QStringList wordList;
    QSet<QString> finalWordSet;
    int conditionNum = 0;

    const int MAX_ANAGRAMS = 65535;
    int minAnagrams = 0;
    int maxAnagrams = MAX_ANAGRAMS;
    int minNumVowels = 0;
    int maxNumVowels = MAX_WORD_LEN;
    int minNumUniqueLetters = 0;
    int maxNumUniqueLetters = MAX_WORD_LEN;
    int minPointValue = 0;
    int maxPointValue = 10 * MAX_WORD_LEN;

    // Look for InWordList conditions first, to narrow the search as much as
    // possible
    QListIterator<SearchCondition> it (spec.conditions);
    while (it.hasNext()) {
        const SearchCondition& condition = it.next();

        // Note the minimum and maximum number of anagrams from any Number of
        // Anagrams conditions
        if (condition.type == SearchCondition::NumAnagrams) {
            if ((condition.minValue > maxAnagrams) ||
                (condition.maxValue < minAnagrams))
                return wordList;
            if (condition.minValue > minAnagrams)
                minAnagrams = condition.minValue;
            if (condition.maxValue < maxAnagrams)
                maxAnagrams = condition.maxValue;
        }

        // Note the minimum and maximum number of vowels from any Number of
        // Vowels conditions
        else if (condition.type == SearchCondition::NumVowels) {
            if ((condition.minValue > maxNumVowels) ||
                (condition.maxValue < minNumVowels))
                return wordList;
            if (condition.minValue > minNumVowels)
                minNumVowels = condition.minValue;
            if (condition.maxValue < maxNumVowels)
                maxNumVowels = condition.maxValue;
        }

        // Note the minimum and maximum number of unique letters from any
        // Number of Unique Letters conditions
        else if (condition.type == SearchCondition::NumUniqueLetters) {
            if ((condition.minValue > maxNumUniqueLetters) ||
                (condition.maxValue < minNumUniqueLetters))
                return wordList;
            if (condition.minValue > minNumUniqueLetters)
                minNumUniqueLetters = condition.minValue;
            if (condition.maxValue < maxNumUniqueLetters)
                maxNumUniqueLetters = condition.maxValue;
        }

        // Note the minimum and maximum point value from any Point Value
        // conditions
        else if (condition.type == SearchCondition::PointValue) {
            if ((condition.minValue > maxPointValue) ||
                (condition.maxValue < minPointValue))
                return wordList;
            if (condition.minValue > minPointValue)
                minPointValue = condition.minValue;
            if (condition.maxValue < maxPointValue)
                maxPointValue = condition.maxValue;
        }

        // Only InWordList conditions allowed beyond this point - look up
        // words for acceptability and combine the word lists
        if (condition.type != SearchCondition::InWordList)
            continue;

        QStringList words = condition.stringValue.split(QChar(' '));
        QSet<QString> wordSet;
        QStringListIterator wit (words);
        while (wit.hasNext()) {
            QString word = wit.next();
            if (isAcceptable(lexicon, word))
                wordSet.insert(word);
        }

        // Combine search result set with words already found
        if (!conditionNum) {
            finalWordSet = wordSet;
        }

        else if (spec.conjunction) {
            QSet<QString> conjunctionSet = finalWordSet & wordSet;
            if (conjunctionSet.isEmpty())
                return wordList;
            finalWordSet = conjunctionSet;
        }

        else {
            finalWordSet += wordSet;
        }

        ++conditionNum;
    }

    // Now limit the set only to those words containing the requisite number
    // of anagrams.  If some words are already in the finalWordSet, then only
    // test those words.  Otherwise, run through the map of number of anagrams
    // and pull out all words matching the conditions.
    if (!finalWordSet.isEmpty() &&
        ((minAnagrams > 0) || (maxAnagrams < MAX_ANAGRAMS)) ||
        ((minNumVowels > 0) || (maxNumVowels < MAX_WORD_LEN)) ||
        ((minNumUniqueLetters > 0) || (maxNumUniqueLetters < MAX_WORD_LEN)) ||
        ((minPointValue > 0) || (minPointValue < 10 * MAX_WORD_LEN)))
    {
        bool testAnagrams = ((minAnagrams > 0) ||
                             (maxAnagrams < MAX_ANAGRAMS));
        bool testNumVowels = ((minNumVowels > 0) ||
                              (maxNumVowels < MAX_WORD_LEN));
        bool testNumUniqueLetters = ((minNumUniqueLetters > 0) ||
                                     (maxNumUniqueLetters < MAX_WORD_LEN));
        bool testPointValue = ((minPointValue > 0) ||
                               (minPointValue < 10 * MAX_WORD_LEN));

        QSet<QString> wordSet;
        QSetIterator<QString> it (finalWordSet);
        while (it.hasNext()) {
            QString word = it.next();

            if (testAnagrams) {
                int numAnagrams = getNumAnagrams(lexicon, word);
                if ((numAnagrams < minAnagrams) || (numAnagrams > maxAnagrams))
                    continue;
            }

            if (testNumVowels) {
                int numVowels = getNumVowels(lexicon, word);
                if ((numVowels < minNumVowels) || (numVowels > maxNumVowels))
                    continue;
            }

            if (testNumUniqueLetters) {
                int numUniqueLetters = getNumUniqueLetters(lexicon, word);
                if ((numUniqueLetters < minNumUniqueLetters) ||
                    (numUniqueLetters > maxNumUniqueLetters))
                    continue;
            }

            if (testPointValue) {
                int pointValue = getPointValue(lexicon, word);
                if ((pointValue < minPointValue) ||
                    (pointValue > maxPointValue))
                    continue;
            }

            wordSet.insert(word);
        }
        finalWordSet = wordSet;
    }

    return finalWordSet.toList();
}

//---------------------------------------------------------------------------
//  addDefinition
//
//! Add a word with its definition.  Parse the definition and separate its
//! parts of speech.
//
//! @param lexicon the name of the lexicon
//! @param word the word
//! @param definition the definition
//---------------------------------------------------------------------------
void
WordEngine::addDefinition(const QString& lexicon, const QString& word,
                          const QString& definition)
{
    if (word.isEmpty() || definition.isEmpty() ||
        !lexiconData.contains(lexicon))
    {
        return;
    }

    QRegExp posRegex (QString("\\[(\\w+)"));
    QMultiMap<QString, QString> defMap;
    QStringList defs = definition.split(" / ");
    foreach (QString def, defs) {
        QString pos;
        if (posRegex.indexIn(def, 0) >= 0) {
            pos = posRegex.cap(1);
        }
        defMap.insert(pos, def);
    }
    lexiconData[lexicon]->definitions.insert(word, defMap);
}

//---------------------------------------------------------------------------
//  getConditionPhase
//
//! Determine the search phase during which a search condition should be
//! considered.
//
//! @param condition the search condition
//! @return the appropriate search phase
//---------------------------------------------------------------------------
WordEngine::ConditionPhase
WordEngine::getConditionPhase(const SearchCondition& condition) const
{
    switch (condition.type) {
        case SearchCondition::AnagramMatch:
        case SearchCondition::SubanagramMatch:
        case SearchCondition::ConsistOf:
        return WordGraphPhase;

        case SearchCondition::Length:
        case SearchCondition::InWordList:
        case SearchCondition::NumVowels:
        case SearchCondition::IncludeLetters:
        case SearchCondition::ProbabilityOrder:
        case SearchCondition::NumUniqueLetters:
        case SearchCondition::PointValue:
        case SearchCondition::NumAnagrams:
        return DatabasePhase;

        case SearchCondition::Prefix:
        case SearchCondition::Suffix:
        case SearchCondition::LimitByProbabilityOrder:
        return PostConditionPhase;

        case SearchCondition::PatternMatch:
        if (condition.stringValue.startsWith("*") &&
            condition.stringValue.endsWith("*") &&
            !condition.stringValue.contains("["))
        {
            return DatabasePhase;
        }
        else
            return WordGraphPhase;

        case SearchCondition::BelongToGroup: {
            SearchSet searchSet =
                Auxil::stringToSearchSet(condition.stringValue);
            if ((searchSet == SetHookWords) || (searchSet == SetFrontHooks) ||
                (searchSet == SetBackHooks))
            {
                return DatabasePhase;
            }
            else
                return PostConditionPhase;
        }

        default:
        return UnknownPhase;
    }
}
