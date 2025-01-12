/******************************************************************************
 * Targoman: A robust Machine Translation framework               *
 *                                                                            *
 * Copyright 2014-2018 by ITRC <http://itrc.ac.ir>                            *
 *                                                                            *
 * This file is part of Targoman.                                             *
 *                                                                            *
 * Targoman is free software: you can redistribute it and/or modify           *
 * it under the terms of the GNU Lesser General Public License as published   *
 * by the Free Software Foundation, either version 3 of the License, or       *
 * (at your option) any later version.                                        *
 *                                                                            *
 * Targoman is distributed in the hope that it will be useful,                *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              *
 * GNU Lesser General Public License for more details.                        *
 * You should have received a copy of the GNU Lesser General Public License   *
 * along with Targoman. If not, see <http://www.gnu.org/licenses/>.           *
 *                                                                            *
 ******************************************************************************/
/**
 * @author S. Mohammad M. Ziabary <ziabary@targoman.com>
 * @author Behrooz Vedadian <vedadian@targoman.com>
 * @author Saeed Torabzadeh <saeed.torabzadeh@targoman.com>
 */

#include <QFile>
#include "IXMLWriter.h"
#include <QRegularExpression>

namespace Targoman {
namespace NLPLibs {
namespace TargomanTP{
namespace Private {

#define TGMN_SUFFIXES "t|ll|ve|s|m|re|d" //these terms can come after apostrofe

IXMLWriter::IXMLWriter() :
    NormalizerInstance(Normalizer::instance()),
    SpellCorrectorInstance(SpellCorrector::instance())
{}

/**
 * @brief Reads abbriviation from file and adds them to #RxAbbrDic
 * @param _configFile abbriviation file address.
 */

void IXMLWriter::init(const QString &_configFile)
{
    this->AbbreviationDetectionRegexPattern = QStringLiteral("");

    QFile AbbrF(_configFile);
    AbbrF.open(QIODevice::ReadOnly);
    QByteArray DataLine;
    int CommentIndex = -1;
    int line = 0;
    while (!AbbrF.atEnd())
    {
        DataLine = AbbrF.readLine().trimmed();
        if (DataLine.isEmpty() || DataLine.startsWith("#"))
            continue;
        if ((CommentIndex = DataLine.indexOf("##")) >= 0)
            DataLine.truncate(CommentIndex);
        if (line > 0) {
            this->AbbreviationDetectionRegexPattern.append("|");
        } else {
            this->AbbreviationDetectionRegexPattern.append("\\b(");
        }
        line++;
        this->AbbreviationDetectionRegexPattern.append(DataLine);
    }

    //this->RxAbbrDic = QRegularExpression (AbbreviationDetectionRegex + ")(?=\\b)");
}

/**
 * @brief finds and tags some patterns in input text and converts them to  ixml format.
 * @param _inStr  input string
 * @param _lang language for input argument of SpellCorrector class.
 * @param _lineNo line number
 * @param _interactive argument of spellCorrector and normalizer. can SpellCorrector or Normalizer class be done interactively or not.
 * @param _useSpellCorrector use spell corrector or not.
 * @param _putXmlTagsInSeperateList put xml tages in _lstXmlTags or put them in text
 * @param _lstXmlTags list of Xml tags
 * @return returns converted ixml text.
 */

QString IXMLWriter::convert2IXML(const QString &_inStr,
                                 bool &_spellCorrected,
                                 bool _putXmlTagsInSeperateList,
                                 QVariantList* _lstXmlTags,
                                 const QList<enuTextTags::Type> _removingTags,
                                 const QString& _lang,
                                 quint32 _lineNo,
                                 bool _interactive,
                                 bool _useSpellCorrector,
                                 bool _setTagValue,
                                 bool _convertToLower,
                                 bool _detectSymbols,
                                 bool _setTagIndex)
{
    // Email detection
    thread_local static QRegularExpression RxEmail = QRegularExpression("([A-Za-z0-9._%+-][A-Za-z0-9._%+-]*@[A-Za-z0-9.-][A-Za-z0-9.-]*\\.[A-Za-z]{2,4})");
    thread_local static QRegularExpression RxAbbrDic;
    
    bool RxAbbrDicIsValid = false;
    if(this->AbbreviationDetectionRegexPattern.length() > 0) {
        RxAbbrDic.setPattern(this->AbbreviationDetectionRegexPattern + ")(?=[^\\w]|$)");
        RxAbbrDicIsValid = true;
    }

    QStringList AllowedFarsiDomainNames = {
        QStringLiteral("کام"),
        QStringLiteral("نت"),
        QStringLiteral("ارگ"),
        QStringLiteral("آی‌آر")
    };

    static QString URLRegExPattern= QStringLiteral("(?:(?:https?|ftp)://)?"
                                                   "(?:(?!10(?:\\.\\d{1,3}){3})"
                                                   //      "(?!127(?:\\.\\d{1,3}){3})"
                                                   //      "(?!169\\.254(?:\\.\\d{1,3}){2})"
                                                   //      "(?!192\\.168(?:\\.\\d{1,3}){2})"
                                                   //      "(?!172\\.(?:1[6-9]|2\\d|3[0-1])(?:\\.\\d{1,3}){2})"
                                                   "(?:[1-9]\\d?|1\\d\\d|2[01]\\d|22[0-3])"
                                                   "(?:\\.(?:1?\\d{1,2}|2[0-4]\\d|25[0-5])){2}"
                                                   "(?:\\.(?:[1-9]\\d?|1\\d\\d|2[0-4]\\d|25[0-4]))"
                                                   "|(?:(?:[a-z\u00a1-\uffff0-9]+-?)*[a-z\u00a1-\uffff0-9]+)"
                                                   "(?:\\.(?:[a-z\u00a1-\uffff0-9]+-?)*[a-z\u00a1-\uffff0-9]+)*"
                                                   "(?:\\.(?:[a-z]{2,4}|") +
                                                   AllowedFarsiDomainNames.join("|") +
                                                   QStringLiteral(")))(?::\\d{2,5})?(?:/[^\\s]*)?");

    thread_local static QRegularExpression RxURL = QRegularExpression(URLRegExPattern, QRegularExpression::CaseInsensitiveOption);

    thread_local static QRegularExpression RxURLValidator = QRegularExpression("^" + URLRegExPattern + "$",  QRegularExpression::CaseInsensitiveOption);

    // Abbreviations
    // thread_local static QRegularExpression RxAbbr = QRegularExpression(QStringLiteral("\\b([A-Z]\\.(?:[A-Z\\d]\\.)(?:[A-Z\\d]\\.)*)(?=[^\\w]|$)"));
    // thread_local static QRegularExpression RxAbbrDotless = QRegularExpression(QStringLiteral("\\b([A-Z]\\.[A-Z\\d](?:\\.[A-Z\\d])*)\\b"));
    thread_local static QRegularExpression RxMultiDots = QRegularExpression(QStringLiteral("(\\.\\.(\\.)*)"));

    // suffixes
    thread_local static QRegularExpression RxSuffix = QRegularExpression(QStringLiteral("((?:'(?:%1))\\b)").arg(TGMN_SUFFIXES));

    //TODO: complete these regexes
    // Dates
    thread_local static QRegularExpression RxDate = QRegularExpression(QStringLiteral("(?:\\b(?:[0-9]{1,4}[/\\\\-][0-9]{1,2}[/\\\\-][0-9]{1,4})\\b)"));
    // Times
    thread_local static QRegularExpression RxTime = QRegularExpression(QStringLiteral("(?:\\b(?:[0-9]{1,2}:[0-9]{1,2}(?::[0-9]{1,2})?)\\b)"));

    // Numbers
    thread_local static QRegularExpression RxDashSeparator = QRegularExpression(QStringLiteral("(\\w)\\-(\\w)"));
    thread_local static QRegularExpression RxUnderlineSeparator = QRegularExpression(QStringLiteral("(\\w)\\_(\\w)"));
    thread_local static QRegularExpression RxSpecialNumber = QRegularExpression(QStringLiteral("(\\d+\\.\\d+\\.(?:\\d+\\.?)*|^\\d+\\.)"));

    //((?:(?:(?:\\b)(:num:))|(?:(?::num:)(?:\\b)))(?=([^\\.\\d]|\\.(?:[^\\d]|$))))
    thread_local static QRegularExpression RxNumberLeft = QRegularExpression("((?:(?:\\b)(" +
                                 QStringLiteral("[\\+\\-]?[0-9]{1,3}[',](?:[0-9]{3}[',])*[0-9]{3}(?:\\.[0-9][0-9]*)?|"
                                                   "[\\+\\-]?[0-9][0-9]*(?:\\.[0-9][0-9]*)?") +
                                 "))(?=([^\\.\\d]|\\.(?:[^\\d]|$))))");

    thread_local static QRegularExpression RxNumberRight = QRegularExpression("(?:([^a-zA-Z0-9])(" +
                                  QStringLiteral("(?:[\\+\\-])?[0-9]{1,3}[',](?:[0-9]{3}[',])*[0-9]{3}(?:\\.[0-9][0-9]*)?|"
                                                    "(?:[\\+\\-])?[0-9][0-9]*(?:\\.[0-9][0-9]*)?") +
                                  +")(?:\\b))(?=([^\\.\\d]|\\.(?:[^\\d]|$)))");

    thread_local static QRegularExpression RxNumberValidator =
            QRegularExpression("^[\\+\\-]?[0-9]{1,3}[',](?:[0-9]{3}[',])*[0-9]{3}(?:\\.[0-9][0-9]*)?|"
                    "[\\+\\-]?[0-9][0-9]*(?:\\.[0-9][0-9]*)?$");

    thread_local static QRegularExpression RxNumbering = QRegularExpression(QStringLiteral("^((?:\\s)*(?:[\"'\\(\\[`])?(?:\\s)*"
                                                  "(?:(?:\\d+)|"
                                                  "(?:M{0,4}(?:CM|CD|D?C{0,3})(?:XC|XL|L?X{0,3})(?:IX|IV|V?I{0,3}))|"
                                                  "(?:الف|[a-zA-Z]|[ابپتثجچهخدذرزژسشصضطظعغفقکگلمنوهی])"
                                                  ")(?:"
                                                  "(?:[\\-\\.])"
                                                  "(?:(?:\\d+)|"
                                                  "(?:M{0,4}(?:CM|CD|D?C{0,3})(?:XC|XL|L?X{0,3})(?:IX|IV|V?I{0,3}))|"
                                                  "(?:الف|[a-zA-Z]|[ابپتثجچهخدذرزژسشصضطظعغفقکگلمنوهی])"
                                                  "))*"
                                                  "(?:[\\-\\.\\s\\)>])"
                                                  ")"));

    thread_local static QRegularExpression RxOrdinalNumber = QRegularExpression(QStringLiteral("((?:\\b)(?:1st|2nd|3rd|\\d+th)(?:\\b))"));
    thread_local static QRegularExpression RxPersianLatin = QRegularExpression(QStringLiteral("([\u0600-\u06ff])(\\d+)?([a-zA-Z])"));
    thread_local static QRegularExpression RxLatinPersian = QRegularExpression(QStringLiteral("([a-zA-Z])(\\d+)?([\u0600-\u06ff])"));
    thread_local static QRegularExpression RxPersianNumber = QRegularExpression(QStringLiteral("([\u0600-\u06ff])(\\d+)"));


    if(_inStr.trimmed().isEmpty())
        return "";
    QString InputPhrase, OutputPhrase;
    InputPhrase += _inStr;

    TargomanDebug(6,"[ORG] "<<InputPhrase);

    //Normalize
    OutputPhrase.clear();
    OutputPhrase.append(" "); // prepend a space before string.

    //normalize input text.
    for (int i=0; i<InputPhrase.size(); i++){
        OutputPhrase.append(this->NormalizerInstance.normalize(
                    InputPhrase.at(i),
                    ((i + 1) < InputPhrase.size() ? InputPhrase.at(i+1) : QChar('\n')),
                    _interactive,
                    _lineNo,
                    InputPhrase,
                    static_cast<size_t>(i)));
    }
    OutputPhrase+=" ."; //append a space and a dot to the end of string for some bug fixings.

    QStringList LstURL;             //list of found URLs
    QStringList LstEmail;           //list of found Emails
    QStringList LstAbbr[3];         //list of found three kind of abbriviations.
    QStringList LstDate;            //list of found Dates.
    QStringList LstTime;            //list of found Times.
    QStringList LstSpecialNumber;   //list of found Special Numbers.
    QStringList LstOrdinal;         //list of found Ordinal Numbers.
    QStringList LstNumberLeft;      //list of found Numbers that were in left side of a word.
    QStringList LstNumberRight;     //list of found Numbers that were in right side of a word.
    QStringList LstSuffixes;        //list of found Suffixes.
    QStringList LstOrderedItem;     //list of found ordered items.
    QStringList LstSymbols;         //list of found symbols.

    TargomanDebug(7,"[NRM] |"<<OutputPhrase<<"|");
    OutputPhrase.replace("&amp;", " & ").replace("&gt;", " > ").replace("&lt;", " < "); //replace '<' a '>' with some special string in order to prevent errors in xml tags.
    TargomanDebug(7,"[NR2] |"<<OutputPhrase<<"|");
    OutputPhrase.replace(RxNumbering, "\\1 ");
    TargomanDebug(7,"[OLI] |"<<OutputPhrase<<"|");
    QStringList PhraseTokens = OutputPhrase.split(" ", QString::SkipEmptyParts);

    // if first token is number we are not sure whether it is for ordered list or not. So we will check it in this if
    if (PhraseTokens.size() && RxNumbering.match(PhraseTokens.first()).hasMatch()){
        if (RxNumberValidator.match(PhraseTokens.first()).hasMatch()){ //check whether first token is a normal number (numbers with optional thousand seperator or decimal numbers )or not.
            LstNumberLeft.append(PhraseTokens.first());
            PhraseTokens[0] = "TGMNNUL";
        }else if (RxURLValidator.match(PhraseTokens.first()).hasMatch()){ //check whether first token is IP of a website or not.
            LstURL.append(PhraseTokens.first());
            PhraseTokens[0] = "TGMNURL";
        }else if (RxAbbrDicIsValid && RxAbbrDic.match(PhraseTokens.first()).hasMatch()){ //check whether first token is in abbreviation dictionary or not.
            LstAbbr[0].append(PhraseTokens.first());
            PhraseTokens[0] = "TGMNABD";
        }else{  // if first token was non of the above, it is ordered list item.
            LstOrderedItem.append(PhraseTokens.first());
            PhraseTokens[0] = "TGMNOLI";
        }
        OutputPhrase = PhraseTokens.join(" ");
    }
    //adds a space between persian word and number
    OutputPhrase.replace(RxPersianNumber, "\\1 \\2");
    TargomanDebug(7,"[P2N] |"<<OutputPhrase<<"|");
    //adds spaces between persian word ,number and latin word.
    OutputPhrase.replace(RxPersianLatin, "\\1 \\2 \\3");
    TargomanDebug(7,"[P2L] |"<<OutputPhrase<<"|");
    // it doesn't add space between latin word and number but adds space between number and persian word.
    OutputPhrase.replace(RxLatinPersian, "\\1\\2  \\3");
    TargomanDebug(7,"[L2P] |"<<OutputPhrase<<"|");

    //find and replace a list patterns.
    OutputPhrase = this->markByRegex(OutputPhrase, RxEmail, "EML", &LstEmail);
    // OutputPhrase = this->markByRegex(OutputPhrase, RxAbbr, "ABR", &LstAbbr[1]);
    // OutputPhrase = this->markByRegex(OutputPhrase, RxAbbrDotless, "ABS", &LstAbbr[2]);
    if(RxAbbrDicIsValid) {
        OutputPhrase = this->markByRegex(OutputPhrase, RxAbbrDic, "ABD", &LstAbbr[0]);
    }
    OutputPhrase = this->markByRegex(OutputPhrase, RxURL, "URL", &LstURL);
    OutputPhrase = this->markByRegex(OutputPhrase, RxMultiDots, "MDT", nullptr);
    OutputPhrase = this->markByRegex(OutputPhrase, RxDate, "DAT", &LstDate);
    OutputPhrase = this->markByRegex(OutputPhrase, RxTime, "TIM", &LstTime);
    OutputPhrase = this->markByRegex(OutputPhrase, RxOrdinalNumber, "ORD", &LstOrdinal);
    OutputPhrase = this->markByRegex(OutputPhrase, RxSpecialNumber, "SNM", &LstSpecialNumber);
    OutputPhrase.replace(RxDashSeparator, "\\1 - \\2"); // adds space before and after dashes in string.
    TargomanDebug(7,"[DSH] |"<<OutputPhrase<<"|");
    OutputPhrase.replace(RxUnderlineSeparator, "\\1 _ \\2"); // adds space before and after underlines in string.
    TargomanDebug(7,"[UND] |"<<OutputPhrase<<"|");
    OutputPhrase = this->markByRegex(OutputPhrase, RxNumberRight,"NUR", &LstNumberRight, 2);
    OutputPhrase = this->markByRegex(OutputPhrase, RxNumberLeft, "NUL", &LstNumberLeft);
    OutputPhrase = this->markByRegex(OutputPhrase, RxSuffix, "SFX", &LstSuffixes);

    //add space before and after non alphaNumeric characters.
    InputPhrase = OutputPhrase;
    OutputPhrase.clear();
    foreach (const QChar& Char, InputPhrase){
        if (!Char.isLetterOrNumber() 
                && Char != ARABIC_ZWNJ
                && Char.category() != QChar::Symbol_Modifier 
                && Char.category() != QChar::Symbol_Math 
                && Char.category() != QChar::Symbol_Other){
            OutputPhrase.append(' ');
            OutputPhrase.append(Char);
            OutputPhrase.append(' ');
        }
        else
            OutputPhrase.append(Char);
    }

    TargomanDebug(7,"[TKN] |"<<OutputPhrase<<"|");

    if (_detectSymbols) {
        //for each token, if any letter of a token is a symbol, add that token to list of symbols and replace whole symbol with " TGMNSYM "
        QStringList Tokens = OutputPhrase.split(" ",QString::SkipEmptyParts);
        for (int i=0; i<Tokens.size(); i++) {
            bool IsSymbol = true;
            foreach (const QChar& Ch, Tokens[i])
                if (Ch.category() != QChar::Symbol_Modifier &&
                        Ch.category() != QChar::Symbol_Math &&
                        Ch.category() != QChar::Symbol_Other){
                    IsSymbol = false;
                    break;
                }
            if (IsSymbol){
                LstSymbols.append(Tokens[i]);
                Tokens[i] = " TGMNSYM ";
            }
        }
        OutputPhrase = Tokens.join(" ");
    }

    TargomanDebug(7,"[SYM] |"<<OutputPhrase<<"|");

    if (_useSpellCorrector)
        OutputPhrase = this->SpellCorrectorInstance.process(
                    _lang,
                    OutputPhrase,
                    _spellCorrected,
                    _interactive);

    TargomanDebug(7,"[SPL] |"<<OutputPhrase<<"|");


    //replace targoman marks with their corresponding words, wrapped with xml tags.
    InputPhrase = OutputPhrase;
    OutputPhrase.clear();

    if(_putXmlTagsInSeperateList)
        _lstXmlTags->clear();

    QSet<enuTextTags::Type> IgnoreTags = QSet<enuTextTags::Type>::fromList(_removingTags);

    enuTextTags::Type TagType;
    QString TagValue;
    bool IsTag;
    QMap<enuTextTags::Type, int> TagCounts;
    foreach (const QString& Token, InputPhrase.split(" ",QString::SkipEmptyParts)) {
        IsTag = true;
        if(Token == "TGMNEML"){
            TagType = enuTextTags::Email;
            TagValue = LstEmail.takeFirst();                
        }
        else if(Token == "TGMNURL"){
            TagType = enuTextTags::URL;
            TagValue = LstURL.takeFirst();
        }
        else if(Token == "TGMNABD"){
            TagType = enuTextTags::Abbreviation;
            TagValue = LstAbbr[0].takeFirst();
        }
        // else if(Token == "TGMNABR"){
        //     TagType = enuTextTags::Abbreviation;
        //     TagValue = LstAbbr[1].takeFirst();
        // }
        // else if(Token == "TGMNABS"){
        //     TagType = enuTextTags::Abbreviation;
        //     TagValue = LstAbbr[2].takeFirst();
        // }
        else if(Token == "TGMNDAT"){
            TagType = enuTextTags::Date;
            TagValue = LstDate.takeFirst();
        }
        else if(Token == "TGMNTIM"){
            TagType = enuTextTags::Time;
            TagValue = LstTime.takeFirst();
        }
        else if(Token == "TGMNORD"){
            //TagType = enuTextTags::Ordinals;
            TagValue = LstOrdinal.takeFirst();            
            if(_convertToLower)
                TagValue = TagValue.toLower();
            OutputPhrase.append(TagValue);
            IsTag = false;
        }
        else if(Token == "TGMNSNM"){
            TagType = enuTextTags::SpecialNumber;
            TagValue = LstSpecialNumber.takeFirst();
        }
        else if(Token == "TGMNNUL"){
            TagType = enuTextTags::Number;
            TagValue = LstNumberLeft.takeFirst();
        }
        else if(Token == "TGMNNUR"){
            TagType = enuTextTags::Number;
            TagValue = LstNumberRight.takeFirst();
        }
        else if(Token == "TGMNOLI"){
            TagType = enuTextTags::OrderedListItem;
            TagValue = LstOrderedItem.takeFirst();
        }
        else if(Token == "TGMNSYM"){
            TagType = enuTextTags::Symbol;
            TagValue = LstSymbols.takeFirst();
        }
        else if(Token == "TGMNMDT"){
            OutputPhrase.append(MULTI_DOT);
            IsTag = false;
        }
        else if(Token == "TGMNSFX"){
            TagValue = LstSuffixes.takeFirst();
            if(_convertToLower)
                TagValue = TagValue.toLower();
            OutputPhrase.append(TagValue);
            IsTag = false;
        }
        else if(Token == "<"){
            OutputPhrase.append("&lt;");
            IsTag = false;
        }
        else if(Token == ">"){
            OutputPhrase.append("&gt;");
            IsTag = false;
        }
        else if(Token == "&"){
            OutputPhrase.append("&amp;");
            IsTag = false;
        }
        else{            
            OutputPhrase.append(_convertToLower
                                    ? Token.toLower()
                                    : Token);
            IsTag = false;                
        }
        if(IsTag){
            if(_convertToLower)
                TagValue = TagValue.toLower();
            if(IgnoreTags.contains(TagType))
                OutputPhrase.append(TagValue);
            else {
                int TagIndex = TagCounts.value(TagType, -1);
                TagIndex++;
                TagCounts.insert(TagType, TagIndex);
                replaceTag(OutputPhrase, TagType, TagValue,_putXmlTagsInSeperateList,_lstXmlTags,_setTagValue, TagIndex, _setTagIndex);
            }
        }
        OutputPhrase.append(" ");
    }
    TargomanDebug(7,"[TRP] |"<<OutputPhrase<<"|");

    OutputPhrase.truncate(OutputPhrase.size() - 2);
    OutputPhrase = this->NormalizerInstance.fullTrim(OutputPhrase.replace("  "," ").replace("  "," "));
    TargomanDebug(7,"[ALL-TAGS] |"<<OutputPhrase<<"|");

    return OutputPhrase;
}

QString IXMLWriter::supportedSuffixes() const
{
    return TGMN_SUFFIXES;
}

/**
 * @brief Finds a RegExp in input _phrase and if found, replaces that with a _mark and adds that to _listOfMaches.
 * @param _phrase input phrase.
 * @param _regex pattern to search for.
 * @param _mark replacement string
 * @param _listOfMatches list to add.
 * @param _capID id of group in regular expression.
 * @return returns replaced string with mark.
 */
QString IXMLWriter::markByRegex(const QString &_phrase,
                                const QRegularExpression& _regex,
                                const QString &_mark,
                                QStringList* _listOfMatches,
                                quint8 _capID)
{
    int Pos=0;
    int Start=0;
    QString OutputPhrase;
    QRegularExpressionMatch Match;

    while((Pos = _phrase.indexOf(_regex, Pos, &Match)) != -1){
        QString A = Match.captured(_capID);
        if (_listOfMatches)
            _listOfMatches->append(Match.captured(_capID));
        OutputPhrase += _phrase.mid(Start,Pos - Start) + " " +
                (_capID == 2 ? Match.captured(1) : "") + " TGMN" + _mark + " ";
        Start = Pos + Match.capturedLength();
        Pos += Match.capturedLength();
    }

    /*while ((Pos = _regex.indexIn(_phrase, Pos)) != -1) {
        QString A = _regex.cap(_capID);
        if (_listOfMatches)
            _listOfMatches->append(_regex.cap(_capID));
        OutputPhrase += _phrase.mid(Start,Pos - Start) + " " +
                (_capID == 2 ? _regex.cap(1) : "") + " TGMN" + _mark + " ";
        Start = Pos + _regex.matchedLength();
        Pos += _regex.matchedLength();
    }
    */

    OutputPhrase += _phrase.mid(Start);

    TargomanDebug(7,"["<<_mark<<"] |"<<OutputPhrase<<"|");
    return OutputPhrase;
}

}
}
}
}
