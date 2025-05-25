/* \file
   \brief Утилзы для marty_csv

 */

#pragma once

#include "utils.h"



//----------------------------------------------------------------------------
// Старое API, где-то используется, поэтому не трогаем

namespace marty_csv {


//----------------------------------------------------------------------------
// https://ru.wikipedia.org/wiki/CSV
// https://datatracker.ietf.org/doc/html/rfc4180
inline
std::string serializeToCsvField(const std::string &s, char sep=';')
{
    bool needQuoting = false;
    for(auto ch: s)
    {
        if (ch=='\"' || ch==sep)
        {
            needQuoting = true;
        }
    }

    if (!needQuoting)
    {
        return s;
    }

    std::string resStr;
    resStr.append(1, '\"');
    for(auto ch: s)
    {
        if (ch=='\"')
        {
            resStr.append(1, '\"');
        }

        resStr.append(1, ch);
    }
    resStr.append(1, '\"');

    return resStr;

}

//----------------------------------------------------------------------------
inline
std::string serializeToCsv(const std::vector< std::vector<std::string> > &csvLines, const std::string &lf="\n", char sep=';')
{
    std::string resStr; resStr.reserve(csvLines.size()*48);

    for(const auto &line: csvLines)
    {
        std::string lineStr;
        for(const auto &field: line)
        {
            if (!lineStr.empty())
            {
                lineStr.append(1, sep);
            }

            lineStr.append(serializeToCsvField(field, sep));
        }

        if (lineStr.empty())
        {
            continue;
        }

        resStr.append(lineStr);
        resStr.append(lf);
    }

    return resStr;
}

//----------------------------------------------------------------------------




//----------------------------------------------------------------------------
//! Разбор CSV данных. CR всегда игнорируем 
inline
std::vector< std::vector<std::string> > deserializeFieldsFromCsvLines(const std::string &str, char sep=';')
{
    std::string                              curField;
    std::vector<std::string>                 curLine;
    std::vector< std::vector<std::string> >  resVec ;


    enum State
    {
        startReading, // start reading field
        readNormal ,
        readQuoted ,
        readQuotedWaitSecondQuot
    };

    State state = startReading;

    auto append = [&](char ch)
    {
        curField.append(1,ch);
    };

    auto addCurField = [&]()
    {
        curLine.emplace_back(curField);
        curField.clear();
        state = startReading;
    };

    auto addCurLine = [&]()
    {
        if (curField.empty() && curLine.empty())
        {
            // Это просто пустая строка, не надо ничего добавлять
            return;
        }

        addCurField(); // finalize field
        resVec.emplace_back(curLine);
        curLine.clear();
    };


    for(char ch : str)
    {
        if (state==startReading) // Только начали читать поле
        {
            if (ch=='\"')
            {
                state = readQuoted; // Меняем состояние на "чтение закавыченного"
                // append(ch); // Саму кавычку пропускаем
            }
            else if (ch==sep) // Нашли разделитель
            {
                // append(ch); // Разделитель игнорируем
                addCurField();
            }
            else if (ch=='\r') // Символ CR - полный игнор
            {
            }
            else if (ch=='\n') // Символ LF - завершение строки
            {
                addCurLine();
            }
            else // Все прочие символы просто добавляем
            {
                append(ch);
            }
        }
        else if (state==readNormal) // Читаем поле без кавычек
        {
            if (ch==sep) // Нашли разделитель
            {
                // append(ch); // Разделитель игнорируем
                addCurField();
            }
            else if (ch=='\r') // Символ CR - полный игнор
            {
            }
            else if (ch=='\n') // Символ LF - завершение строки
            {
                addCurLine();
            }
            else // Все прочие символы просто добавляем
            {
                append(ch);
            }
        }
        else if (state==readQuoted) // Читаем поле в кавычках
        {
            if (ch=='\"')
            {
                state = readQuotedWaitSecondQuot; // Меняем состояние на "ожидание дубля кавычки"
                // append(ch); // Саму кавычку пропускаем
            }
            // Разделитель игнорируем как пазделитель, он внутри закавыченного поля - как простой символ
            else if (ch=='\r') // Символ CR - полный игнор
            {
            }
            else if (ch=='\n') // Символ LF - завершение строки, но внутри закавыченного поля это означает перенос строки внутри поля
            {                  // Просто добавляем как все прочие символы в текущее поле
                append(ch);
            }
            else // Все прочие символы просто добавляем
            {
                append(ch);
            }
        }
        else if (state==readQuotedWaitSecondQuot) // Читаем поле в кавычках, ожидаем кавычку или разделитель полей
        {
            if (ch=='\"')
            {
                state = readQuoted; // Меняем состояние на "чтение закавыченного"
                append(ch); // Кавычку добавляем
            }
            else if (ch==sep) // Нашли разделитель
            {
                // append(ch); // Разделитель игнорируем
                addCurField();
            }
            else // Все прочие символы просто добавляем
            {
                append('\"'); // одиночная кавычка внутри закавыченного поля - работает как одиночная кавычка
                append(ch);
                state = readQuoted;
            }

        }
        else
        {
            // По идее такого не должно, тут наверное надо что-то кинуть
        }
    
    } // for(char ch : str)


    addCurLine(); // Добавляем, если не пусто


    return resVec;

}

//----------------------------------------------------------------------------
template<typename ToWideConverter> inline
std::vector< std::vector<std::wstring> > csvFieldsToWide(const std::vector< std::vector<std::string> > &lines, ToWideConverter converter)
{
    std::vector< std::vector<std::wstring> > recVec; recVec.reserve(lines.size());

    for(const auto &line: lines)
    {
        std::vector<std::wstring> wl; wl.reserve(line.size());
        for(const auto &field: line)
        {
            wl.emplace_back(converter(field));
        }

        recVec.emplace_back(wl);
    }

    return recVec;
}

//----------------------------------------------------------------------------
template<typename ToAsciiConverter> inline
std::vector< std::vector<std::string> > csvFieldsToAnsi(const std::vector< std::vector<std::wstring> > &lines, ToAsciiConverter converter)
{
    std::vector< std::vector<std::string> > recVec; recVec.reserve(lines.size());

    for(const auto &line: lines)
    {
        std::vector<std::string> wl; wl.reserve(line.size());
        for(const auto &field: line)
        {
            wl.emplace_back(converter(field));
        }

        recVec.emplace_back(wl);
    }

    return recVec;
}

//----------------------------------------------------------------------------



//----------------------------------------------------------------------------

} // namespace marty_csv


#include "marty_csv_new.h"



