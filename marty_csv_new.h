/* \file
   \brief marty_csv_new - новое API для работы с CSV - marty::csv

 */

#pragma once

#include <algorithm>
#include <string>
#include <vector>


namespace marty {
namespace csv {

//----------------------------------------------------------------------------



//----------------------------------------------------------------------------
enum class ParseErrorType
{
    UnclosedQuote,
    InvalidCharAfterQuote,
    InconsistentColumns,
    InvalidQuoteUsage
};

inline
std::string to_string(ParseErrorType peType)
{
    switch(peType)
    {
        case ParseErrorType::UnclosedQuote        : return "UnclosedQuote";
        case ParseErrorType::InvalidCharAfterQuote: return "InvalidCharAfterQuote";
        case ParseErrorType::InconsistentColumns  : return "InconsistentColumns";
        case ParseErrorType::InvalidQuoteUsage    : return "InvalidQuoteUsage";
        default: return "Unknown";
    }
}

struct ParseError
{
    ParseErrorType type;
    std::string message;
    size_t line;
    size_t position;
};

template<typename StreamType>
void printError(StreamType &oss, const ParseError &pe)
{
    using std::to_string;

    oss << pe.line << ":" << pe.position << ": " << to_string(pe.type) << ": " << pe.message; // << "\n";
}

struct ParseResult
{
    std::vector<std::vector<std::string>> data;
    std::vector<ParseError>               errors;
};

//----------------------------------------------------------------------------



//----------------------------------------------------------------------------
namespace details {


//----------------------------------------------------------------------------
struct CLineCharCounts
{
    int counts[128];
};

//----------------------------------------------------------------------------
struct CVarianceItem
{
    int D;
    int M;
    unsigned ch;
    // unsigned tag; // 0/1/2 - Simple/Excel/Ex line split method
    // static const unsigned usedUnquotedOnly    = 0x10;
    // static const unsigned lineSplitMethodMask = 0x0F;

    CVarianceItem() : D(100000000), M(0), ch(0) /* , tag(0) */  {}
    CVarianceItem(char c) : D(100000000), M(0), ch((unsigned)c) /* , tag(0) */  {}
    CVarianceItem(int d, int m, char c) : D(d), M(m), ch((unsigned)c) /* , tag(0) */  {}
    CVarianceItem(int d, int m, unsigned c) : D(d), M(m), ch(c)/*, tag(0)*/ {}
    //CVarianceItem(int d, int m, char c     /* , unsigned t */ ) : D(d), M(m), ch((unsigned)c) /* , tag(t) */  {}
    //CVarianceItem(int d, int m, unsigned c /* , unsigned t */ ) : D(d), M(m), ch(c) /* , tag(t) */  {}

    bool operator<(const CVarianceItem &v) const { return D<v.D; }
    bool operator>(const CVarianceItem &v) const { return D>v.D; }
};

//----------------------------------------------------------------------------



//----------------------------------------------------------------------------
template< typename CharType>
unsigned makeIndexFromChar( CharType ch )
{
    return (unsigned)ch;
}

template<> unsigned makeIndexFromChar<char>( char ch )
{
    return (unsigned)(unsigned char)ch;
}

template<> unsigned makeIndexFromChar<wchar_t>( wchar_t ch )
{
    return (unsigned)ch;
}

//----------------------------------------------------------------------------



//----------------------------------------------------------------------------
template<typename StringType>
void
calcLineCharCounts( const StringType &str
                  , CLineCharCounts &lineCharCounts
                  , CLineCharCounts &totalCharCounts
                  )
{
    typedef typename StringType::size_type    size_type ;
    typedef typename StringType::value_type   value_type;

    size_type i = 0, size = str.size();
    for(; i!=size; ++i)
    {
        std::size_t idx = (std::size_t)(unsigned char)str[i];
        if (idx>126) continue;
        ++lineCharCounts.counts[idx];
        ++totalCharCounts.counts[idx];
    }
}

//----------------------------------------------------------------------------
template<typename LinesVectorType, typename LineCharCountsVectorType>
void calcCharCountsForLines( const LinesVectorType    &lines
                           , LineCharCountsVectorType &lineCharCounts // new vals wil be pushed to back
                           , CLineCharCounts          &totalCharCounts
                           )
{
    //CLineCharCounts
    lineCharCounts.clear();
    lineCharCounts.reserve(lines.size());

    auto lit = lines.begin();
    for(; lit != lines.end(); ++lit)
    {
        CLineCharCounts lcc = { 0 };
        calcLineCharCounts        ( *lit, lcc, totalCharCounts );
        lineCharCounts.push_back(lcc);
    }
}

//----------------------------------------------------------------------------
inline
void calcCharsMathEV( std::size_t           numLines
                    , const CLineCharCounts &totalCharCounts
                    , CLineCharCounts       &charsMathEV
                    )
{
    for(std::size_t i=0; i<127; ++i)
        charsMathEV.counts[i] = !numLines ? 0 : 100*totalCharCounts.counts[i] / (int)numLines;
}

//----------------------------------------------------------------------------
template<typename LineCharCountsVectorType>
void calcVariance( const LineCharCountsVectorType &lineCharCounts
                 , const CLineCharCounts          &charsMathEV
                 , const CLineCharCounts          &totalCharCounts
                 , CLineCharCounts                &charsVariance
                 )
{
    for(std::size_t i=0; i<127; ++i)
    {
        charsVariance.counts[i] = 0;
    }

    std::size_t lci = 0;
    std::size_t size = lineCharCounts.size();
    for(; lci != size; ++lci)
    {
        for(std::size_t i=0; i<127; ++i)
        {
            int v1 = 100*lineCharCounts[lci].counts[i] - charsMathEV.counts[i];
            charsVariance.counts[i] += v1*v1;
        }
    }

    for(std::size_t i=0; i<127; ++i)
    {
        if (totalCharCounts.counts[i]==0) { charsVariance.counts[i] = 100000000; continue; }

        charsVariance.counts[i] = lineCharCounts.empty() ? 0 : charsVariance.counts[i] / (int)lineCharCounts.size();
    }
}

//----------------------------------------------------------------------------
template<typename LinesVectorType>
void calcVariance( const LinesVectorType &lines
                 , CLineCharCounts       &charsMathEV
                 , CLineCharCounts       &charsVariance
                 )
{
    std::vector<CLineCharCounts> lineCharCounts;
    CLineCharCounts totalCharCounts = { 0 };
    //CLineCharCounts charsVariance   = { 0 };
    calcCharCountsForLines( lines, lineCharCounts, totalCharCounts);

    //CLineCharCounts charsMathEV = { 0 };
    calcCharsMathEV( (unsigned)lines.size(), totalCharCounts, charsMathEV );

    calcVariance( lineCharCounts, charsMathEV, totalCharCounts, charsVariance );
}

//----------------------------------------------------------------------------
inline
void getCharVarianceOrderedVectorRequestedChars( const std::vector< CVarianceItem > &vvFrom
                                               , std::vector< CVarianceItem > &vvTo
                                               , const char *pRequestedChars
                                               )
{
    if (!pRequestedChars) return;
    vvTo.clear();
    std::vector< CVarianceItem >::const_iterator vvIt = vvFrom.begin();
    for(; vvIt != vvFrom.end(); ++vvIt)
       {
        for(const char *pCh = pRequestedChars; *pCh; ++pCh)
           {
            if ( vvIt->ch==makeIndexFromChar(*pCh) ) 
               vvTo.push_back(*vvIt);
           }
       }
}

inline
void buildCharVarianceOrderedVector( const CLineCharCounts &charsMathEV
                                   , const CLineCharCounts &charsVariance
                                   , std::vector< CVarianceItem > &vv
                                   //, unsigned    tag
                                   , const char *pRequestedChars = 0
                                   )
{
    //vv.clear(); 
    vv.reserve(127);
    for(unsigned i=0; i<127; ++i)
       {
        vv.push_back( CVarianceItem(charsVariance.counts[i], charsMathEV.counts[i], i /* , tag */  ) );
       }

    std::vector< CVarianceItem >::iterator vvIt0 = vv.begin();
    for(; vvIt0 != vv.end(); ++vvIt0)
       {
        if ( vvIt0->M < 100 ) vvIt0->D = 100000000;
       }

    //std::sort(vv.begin(), vv.end(), std::less<CVarianceItem>() );
    std::sort(vv.begin(), vv.end() );
    if (!pRequestedChars) return;

    std::vector< CVarianceItem > requestedItemsOnly;
    getCharVarianceOrderedVectorRequestedChars( vv, requestedItemsOnly, pRequestedChars );

/*
    std::vector< CVarianceItem >::const_iterator vvIt = vv.begin();
    for(; vvIt != vv.end(); ++vvIt)
       {
        for(const char *pCh = pRequestedChars; *pCh; ++pCh)
           {
            if ( vvIt->ch==(unsigned)(unsigned char)*pCh ) requestedItemsOnly.push_back(*vvIt);
           }
       }
*/
    vv.swap(requestedItemsOnly);
}

template<typename StringVectorType>
void getCsvSeparatorVariances( std::vector< CVarianceItem > &vv
                             , const StringVectorType       &lines
                             , const char                   *pRequestedChars = 0
                             )
{

    //if (linesSimple.size()>1)
       {
          {
            CLineCharCounts charsMathEV, charsVariance;
            calcVariance( lines, charsMathEV, charsVariance );
            buildCharVarianceOrderedVector( charsMathEV, charsVariance, vv, pRequestedChars );
          }
       }

}

//----------------------------------------------------------------------------
//! Разделяет входной текст на строки, разделитель CR LF или LF - для CSV сойдёт
template<typename IterType, typename StringVectorType>
void splitToLinesBasicImpl(IterType b, IterType e, StringVectorType &lines)
{
    using StringType = typename StringVectorType::value_type;
    StringType buf;

    // for(auto ch : str)
    for(; b!=e; ++b)
    {
        auto ch = *b;

        if (ch=='\r')
            continue; // Просто игнорим

        if (ch=='\n')
        {
            lines.emplace_back(buf);
            buf.clear();
            continue;
        }    

        buf.append(1, ch);
    }

    if (!buf.empty())
         lines.emplace_back(buf);
}

//----------------------------------------------------------------------------
//! Разделяет входной текст на строки, разделитель CR LF или LF - для CSV сойдёт
//! Символ кавычек уже должен быть продетекчен
template<typename IterType, typename StringVectorType>
void splitToLines(IterType b, IterType e, StringVectorType &lines, char quot=0)
{
    using StringType = typename StringVectorType::value_type;

    splitToLinesBasicImpl(b, e, lines);

    StringVectorType tmp; tmp.reserve(lines.size());

    // На коленке детектим поля с переносом строки. Нам это нужно, чтобы более корректно детектить разделители, не более того,
    // так что возможные неверные детекты не будут проблемой

    static const std::string seps   = "\t;,:|#";
    // constexpr static const std::string quotes = "\"\'`";

    if (quot==0)
        quot = '\"';

    for(auto &&l : lines)
    {
        auto sepPos  = l.find_first_of(seps);
        
        if (sepPos==l.npos)
        {
            // Разделитель не найден, не важно, есть ли кавычки - считаем,
            // что это либо середина, либо окончание разделённой на несколько строк ячейки
            if (tmp.empty())
            {
                tmp.push_back(l);
            }
            else
            {
                tmp.back().append(StringType("\n"));
                tmp.back().append(l);
            }
        }
        else // есть разделитель
        {
            auto quotPos = l.find(quot);
            if (quotPos==l.npos)
            {
                // а кавычки нет - обычная строка?
                tmp.push_back(l);
            }
            else
            {
                // есть разделитель и кавычка
                if (quotPos<sepPos) // Кавычка раньше разделителя
                {
                    if (tmp.empty())
                    {
                        tmp.push_back(l);
                    }
                    else
                    {
                        tmp.back().append(StringType("\n"));
                        tmp.back().append(l);
                    }
                }
                else // Разделитель раньше - тут может быть косяк, например, с запятой, но ничего не поделать
                {
                    tmp.push_back(l);
                }
            }
        }
    }

    swap(tmp, lines);

}

//----------------------------------------------------------------------------

template<typename IterType>
IterType getChunkForDetections(IterType b, IterType e, std::size_t chunkSize=1000*1000)
{
    for(std::size_t i=0u; b!=e && i!=chunkSize; ++b) {}

    if (b==e)
        return b;

    // Хотим найти перевод строки, но ищем его не очень далеко

    for(std::size_t i=0u; b!=e && i!=1000; ++b)
    {
        if (*b=='\n' || *b=='\r')
            break;
    }

    return b;
}

//----------------------------------------------------------------------------



//----------------------------------------------------------------------------
class CsvParser
{
    char m_delimiter      = 0;
    char m_quot           = 0;
    bool m_strictMode     = false;
    size_t m_currentLine  = 1;
    size_t m_currentPos   = 0;
    size_t m_lineStartPos = 0;

    void addError(ParseResult& result, ParseErrorType type, const std::string& msg)
    {
        using std::to_string;

        auto linePos = m_currentPos - m_lineStartPos + 1;

        result.errors.push_back(
            {
                type,
                msg // + " (line: " + to_string(m_currentLine) + 
                    // ", pos: " + to_string(linePos) + ")"
                    ,
                m_currentLine,
                linePos
            });
    }

    static
    std::string trimRight(const std::string &str)
    {
        size_t lastNonSpace = str.find_last_not_of(" \t");
        if (lastNonSpace!=std::string::npos)
            return str.substr(0, lastNonSpace + 1);
        return str;
    }

    static
    std::string trimLeft(const std::string &str)
    {
        size_t firstrNonSpace = str.find_first_not_of(" \t");
        if (firstrNonSpace!=std::string::npos)
            return str.substr(firstrNonSpace, str.npos);
        return std::string(); // Непробельные символы не найдены - у нас пустая строка
    }

    static
    std::string trim(const std::string &str)
    {
        return trimLeft(trimRight(str));
    }


public:

    CsvParser(char delim=',', char quot='\"', bool strict=true)
    : m_delimiter(delim)
    , m_quot(quot)
    , m_strictMode(strict)
          // m_currentLine(1),
          // m_currentPos(0)
    {
        if (!m_delimiter)
            m_delimiter = ';';

        if (!m_quot)
             m_quot = '\"';
    }

    ParseResult parse(const std::string& content)
    {
        using std::to_string;

        ParseResult result;
        std::vector<std::string> currentRow;
        std::string field;
        bool inQuotes = false;
        bool wasQuoted = false;
        size_t columnsCount = 0;
        bool lastCharDelimiter = false;


        // m_currentLine = 1;
        // m_currentPos = 0;
        //size_t 
        m_lineStartPos = 0;

        auto addCol = [&]()
        {
            if (!wasQuoted)
                field = trim(field);
            currentRow.push_back(field);
            field.clear();
            inQuotes = false;
            wasQuoted = false;
        };

        auto handleRowEnd = [&](bool force = false)
        {
            if (force || !inQuotes)
            {
                if (wasQuoted)
                {
                    // size_t lastNonSpace = field.find_last_not_of(" \t");
                    // if (lastNonSpace != std::string::npos)
                    // {
                    //     field = field.substr(0, lastNonSpace + 1);
                    // }

                    field = trimRight(field);
                }

                if (!field.empty() || lastCharDelimiter || wasQuoted)
                {
                    // if (!wasQuoted)
                    //     field = trim(field);
                    // currentRow.push_back(field);
                    addCol();
                }

                if (!currentRow.empty())
                {
                    if (columnsCount == 0)
                    {
                        columnsCount = currentRow.size();
                    }
                    else if (m_strictMode && currentRow.size() != columnsCount)
                    {
                        addError(result, ParseErrorType::InconsistentColumns,
                            "Columns count mismatch. Expected: " + 
                            to_string(columnsCount) + 
                            ", got: " + to_string(currentRow.size()));
                    }
                    result.data.push_back(currentRow);
                }

                currentRow.clear();
                field.clear();
                inQuotes = false;
                wasQuoted = false;
                m_currentLine++;
                m_lineStartPos = m_currentPos + 1;
                return true;
            }
            return false;
        };

        for (m_currentPos = 0; m_currentPos < content.size(); ++m_currentPos)
        {
            char c = content[m_currentPos];
            
            if (inQuotes)
            {
                if (c == m_quot)
                {
                    if (m_currentPos + 1 < content.size() && content[m_currentPos + 1] == m_quot)
                    {
                        field += m_quot;
                        m_currentPos++;
                    }
                    else
                    {
                        inQuotes = false;
                        wasQuoted = true;
                        
                        size_t end = m_currentPos + 1;
                        while (end < content.size() && 
                               content[end] != m_delimiter && 
                               content[end] != '\r' && 
                               content[end] != '\n')
                        {
                            if (content[end] != ' ' && content[end] != '\t')
                            {
                                addError(result, ParseErrorType::InvalidCharAfterQuote, "Invalid character after closing quote");
                                while (end < content.size() && 
                                       content[end] != m_delimiter && 
                                       content[end] != '\n' && 
                                       content[end] != '\r')
                                {
                                    end++;
                                }
                                m_currentPos = end - 1;
                                break;
                            }
                            end++;
                        }
                        m_currentPos = end - 1;
                    }
                }
                else
                {
                    field += c;
                }
            }
            else
            {
                if (c == m_quot)
                {
                    if (!field.empty() && field != std::string(field.size(), ' '))
                    {
                        addError(result, ParseErrorType::InvalidQuoteUsage, "Quote appears in middle of field");
                        field += c;
                    }
                    else
                    {
                        inQuotes = true;
                        wasQuoted = true;
                        field.clear();
                    }

                    lastCharDelimiter = false;
                }
                else if (c == m_delimiter)
                {
                    // currentRow.push_back(field);
                    // field.clear();
                    // wasQuoted = false;
                    addCol();
                    lastCharDelimiter = true;
                }
                else if (c == '\r' || c == '\n')
                {
                    bool lineEndProcessed = handleRowEnd();
                    
                    while (m_currentPos + 1 < content.size() && 
                          (content[m_currentPos + 1] == '\r' || 
                           content[m_currentPos + 1] == '\n'))
                    {
                        m_currentPos++;
                    }
                    
                    if (!lineEndProcessed)
                    {
                        addError(result, ParseErrorType::UnclosedQuote, "Unclosed quotes in field. Attempting recovery");
                        handleRowEnd(true);
                    }

                    lastCharDelimiter = false;
                }
                else
                {
                    field += c;
                    lastCharDelimiter = false;
                }
            }
        }

        if (!field.empty() || lastCharDelimiter || wasQuoted || !currentRow.empty())
        {
            if (inQuotes)
            {
                addError(result, ParseErrorType::UnclosedQuote, "Unclosed quotes at end of input");
            }
            handleRowEnd(true);
        }

        return result;
    }
};





//----------------------------------------------------------------------------

} // namespace details

//----------------------------------------------------------------------------




//----------------------------------------------------------------------------
//! Автоопределение кавычек. Учитываются варианты, когда кавычка следует за разделителем полей, и наоборот
//! Не самый надёжный способ, зато не запарный
template<typename InputIter, typename StringType>
char detectQuotes(InputIter b, InputIter e, const StringType &seps="\t;,:|#", const StringType &quotes="\"\'`", std::size_t detectChunkSize=1000*1000)
{
    e = details::getChunkForDetections(b, e, detectChunkSize);

    std::vector<std::size_t> quotesCount = std::vector<std::size_t>(quotes.size(), 0);

    bool prevSep            = false;
    //bool prevQuot           = false;
    std::size_t prevQuotIdx = StringType::npos;

    // for(auto ch : data)
    for(; b!=e; ++b)
    {
        auto ch = *b;

        auto sepIdx  = seps.find(ch);
        auto quotIdx = quotes.find(ch);

        if (sepIdx==StringType::npos)
        {
            // Разделитель не найден

            if (quotIdx!=StringType::npos) // найдена одна из кавычек
            {
                if (prevSep) // предыдущий символ - один из разделителей
                {
                    if (quotIdx<quotesCount.size())
                        quotesCount[quotIdx]++;
                }
            }
            else if (prevQuotIdx!=StringType::npos) // Предыдуший символ - кавычка
            {
                if (ch=='\n' || ch=='\r')
                {
                    if (quotIdx<quotesCount.size())
                        quotesCount[quotIdx]++;
                }
            }

            prevSep = false;
            prevQuotIdx = quotIdx;
        }
        else // найден один из разделителей
        {
            if (prevQuotIdx!=StringType::npos) // Предыдущий символ - одна из кавычек
            {
                if (prevQuotIdx<quotesCount.size())
                    quotesCount[prevQuotIdx]++;
            }

            prevSep = true;
            prevQuotIdx = quotIdx;
        }
    }

    std::size_t maxIdx = std::size_t(-1);
    std::size_t maxQuotCount = 0;

    
    for(std::size_t i=0u; i!=quotesCount.size(); ++i)
    {
        auto cnt = quotesCount[i];
        if (cnt>maxQuotCount)
        {
            maxQuotCount = cnt;
            maxIdx = i;
        }
    }

    if (maxIdx==std::size_t(-1) || maxQuotCount==0 || maxIdx>=quotes.size())
        return 0;

    return quotes[maxIdx];

}

//----------------------------------------------------------------------------
template<typename StringType>
char detectQuotes(const StringType &data, const StringType &seps="\t;,:|#", const StringType &quotes="\"\'`", std::size_t detectChunkSize=1000*1000)
{
    return detectQuotes(data.begin(), data.end(), seps, quotes, detectChunkSize);
}

//----------------------------------------------------------------------------
template<typename IterType, typename StringType>
char detectSeparators(IterType b, IterType e, const StringType &seps="\t;,:|#", typename StringType::value_type quot=0, std::size_t detectChunkSize=1000*1000)
{
    e = details::getChunkForDetections(b, e, detectChunkSize);

    if (quot==0)
        quot = '\"';

    std::vector<StringType> lines; lines.reserve(std::size_t(std::distance(b,e))/100u);
    details::splitToLines(b, e, lines, quot);

    std::vector< details::CVarianceItem > vv;
    details::getCsvSeparatorVariances(vv, lines, seps.c_str());
    if (!vv.empty())
        return char(vv[0].ch);

    return char(0);
}

//----------------------------------------------------------------------------
template<typename StringType>
char detectSeparators(const StringType &data, const StringType &seps="\t;,:|#", typename StringType::value_type quot=0, std::size_t detectChunkSize=1000*1000)
{
    return detectSeparators(data.begin(), data.end(), seps, quot, detectChunkSize);
}

//----------------------------------------------------------------------------
inline
ParseResult parse(const std::string& content, char delim=',', char quot='\"', bool strict=true)
{
    auto parser = details::CsvParser(delim, quot, strict);
    return parser.parse(content);
}

//----------------------------------------------------------------------------


//----------------------------------------------------------------------------
namespace utils {

//----------------------------------------------------------------------------
inline
std::string expandStr(std::string str, std::size_t size)
{
    str.resize(size, ' ');
    return str;
}

//----------------------------------------------------------------------------



//----------------------------------------------------------------------------

} // namespace utils

//----------------------------------------------------------------------------



//----------------------------------------------------------------------------

} // namespace csv
} // namespace marty


