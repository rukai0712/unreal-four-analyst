#include<iostream>
#include<unordered_set>
#include<regex>
#include<string>
#include<utility>
#include<vector>
#include<cassert>
#include"CodeElement.h"
//!!!!
#include"wrap.hpp"

using namespace std;
using namespace RKHT;


/* TODO 比较粗糙,正确的做法是简历忽略的set，然后匹配
 *  对不需要的word进行剔除，如果能够忽略，那么返回ture，否则返回false
 *
 */
bool Ignored(std::string const& word)
{
    /*
     *  std_class_set 保存了
     *  在Ignore中保留的标准数据类型
     */
    static unordered_set<string> const std_class_set
    {
        "string", "pair", "vector", "map", "set","array"
    };

    if (word.size() <= 1)
    {
        return true;
    }
    //如果是std中特定几个数据类型那么不忽略
    if (std_class_set.end() != std_class_set.find(word))
    {
        return false;
    }
    //TODO 比较粗糙的设计
    //UNREAL 中变量名，类名等都是大写字母开头
    if (isupper(word[0]) || word[0] == '_')
    {
        return false;
    }
    return true;
}

bool Ignored(std::smatch const& match)
{
    return Ignored(match.str());
}

bool Ignored(std::cmatch const& match)
{
    return Ignored(match.str());
}


/* TESTED~~~
 *  用来切分，将最外部的括号的作用于取出来
 *  用来处理括号层层嵌套的问题
 *  返回字符的返回指针
 */
template <char LBracket, char RBracket>
    std::pair<std::string::const_iterator, std::string::const_iterator>
FindBracketRange(std::string::const_iterator str_begin, std::string::const_iterator str_end)
{

    if (str_begin >= str_end)
    {
        return std::make_pair(str_end, str_end);
    }
    std::string bracket_str{'[', '\\', LBracket, '\\', RBracket, ']'};
    regex bracket_regex(bracket_str);

    std::string::const_iterator bracket_range_begin = str_begin;
    std::string::const_iterator bracket_range_end = str_begin;

    smatch bracket_match;
    int left_bracket_count = 0;
    std::sregex_iterator bracket_iter(str_begin, str_end, bracket_regex);
    std::sregex_iterator siter_end;
    //寻找第一个LBracket
    while (bracket_iter != siter_end && *((*bracket_iter)[0].first) != LBracket)
    {
        ++bracket_iter;
    }
    if (bracket_iter == siter_end)
    {
        //并没有找到
        return std::make_pair(str_end, str_end);
    }
    bracket_match = *bracket_iter;
    bracket_range_begin = bracket_match[0].first;
    ++left_bracket_count;
    ++bracket_iter;
    while (bracket_iter != siter_end && left_bracket_count > 0) 
    {
        bracket_match = *bracket_iter;
        if (*bracket_match[0].first == LBracket)
        {
            ++left_bracket_count;
        }
        else if (*bracket_match[0].first == RBracket)
        {
            --left_bracket_count;
        }
        ++bracket_iter;
    } 
    if (left_bracket_count != 0)
    {
        return std::make_pair(str_end, str_end);
    }
    bracket_range_end = bracket_match[0].second;
    return std::make_pair(std::move(bracket_range_begin), std::move(bracket_range_end));
}


/*
 *  返回是否是class, struct, union, enum, typedef, using, namespace, friend 关键词
 */
bool IsKeyWord(std::string const& word)
{
    /*
     *  std_class_set 保存了
     *  在Ignore中保留的标准数据类型
     */
    static unordered_set<string> const key_word_set
    {
        "class", "struct", "union", "enum", "typedef", "using", "namespace", "friend"
    };
    //如果是std中特定几个数据类型那么不忽略
    if (key_word_set.end() != key_word_set.find(word))
    {
        return true;
    }
    return false;
}

bool IsKeyWord(std::smatch const& match)
{
    return IsKeyWord(match.str());
}

bool IsKeyWord(std::cmatch const& match)
{
    return IsKeyWord(match.str());
}

bool IsEof(cregex_token_iterator* fragment_slider_ptr)
{
    static cregex_token_iterator const fragment_end;
    if ( *fragment_slider_ptr == fragment_end)
    {
        return true;
    }
    return false;
}

bool IsEmpty(cregex_token_iterator* fragment_slider_ptr)
{
    static regex const empty_regex("[[:s:]]");
    return regex_match((*fragment_slider_ptr)->first, (*fragment_slider_ptr)->second, empty_regex);
}

/*
 *  ICodeElement的工厂类
 */

/*
 * 用来分析片段，产生元素
 * 需要处理的内容包括：
 * 1.根据语句创建各个元素
 * 2.丢弃public 和 private inline static 等关键字
 * 3.丢弃友元片段
 * 4.如果是类中函数的定义，那么需要舍弃
 * 如果当前片段无效，返回空值
 */
//TODO 未测试 
static shared_ptr<ICodeElement> CreateElementFactory(cregex_token_iterator* fragment_slider_ptr)
{
    assert(fragment_slider_ptr != nullptr);
    if (IsEof(fragment_slider_ptr))
    {
        return nullptr;
    }

    if (IsEmpty(fragment_slider_ptr))
    {
        ++(*fragment_slider_ptr);
        return nullptr;
    }

    auto fragment_match = **fragment_slider_ptr;
    //先拷贝当前句段
    std::string sentence(" ");
    sentence.append(fragment_match.first, fragment_match.second);
    ++(*fragment_slider_ptr);
    fragment_match = **fragment_slider_ptr;
    //提取出是一个语句还是一个结构
    char break_character;
    if (*fragment_match.first == '{')
    {
        break_character = '}';
        ++(*fragment_slider_ptr);
    }
    else if (*fragment_match.first == ';')
    {
        break_character = ';';
    }
    else
    {
        cerr<< sentence<< endl;
        ErrorExit("fragment_slider_ptr != (; || {) in \"%s\"", fragment_match.str().c_str());
    }

    //策略：对关键字进行寻找
    //对宏函数进行剔除, 然后进行分析

    //    static regex const macro_regex("\\s+([A-Z_0-9]{2,}(?:\\s*\\(.*\\))?)\\s+");
    static regex const macro_func_regex("\\s([A-Z_0-9]{2,}\\s*\\(.*\\))");
    sregex_iterator macro_func_iter(sentence.begin(), sentence.end(), macro_func_regex);
    sregex_iterator siter_end;
    smatch macro_func_match;
    while (macro_func_iter != siter_end)
    {
        macro_func_match = *macro_func_iter;
        sentence.replace(macro_func_match[1].first, macro_func_match[1].second, ' ', 1);
        ++macro_func_iter;
    }

    //对任何一个符号出现之前的words 进行分析
    //
    auto key_search_begin = sentence.cbegin();
    auto key_search_end = sentence.cend();
    //除‘_’和‘:’之外的符号
    //‘_’作为下标
    //‘:’作为public 或 private 后的修饰符
    static regex const punct_regex("[\\(\\)\\{\\}\\[\\]\\<\\>\\=\"\'\\~\\*\\&\\.\\,\\#\\|\\%\\+\\$\\@\\-\\/]");
    smatch punct_match;
    if (regex_search(key_search_begin, key_search_end, punct_match, punct_regex))
    {
        key_search_end = punct_match[0].first;
    }

    static regex const wd_regex("\\w+");
    sregex_iterator wd_slider;

    //1. template匹配
    bool is_template = false;
    std::set<std::string> template_typename_set;
    static regex const template_regex("\\s(template)(?:\\s.*)?$");
    smatch template_match;
    if (regex_search(key_search_begin, key_search_end, template_match, template_regex))
    {
        is_template = true;
        key_search_end = sentence.cend();
        //找到模板的参数列表
        auto parameter_list_range = FindBracketRange<'<','>'>(template_match[1].second, key_search_end);

        assert (parameter_list_range.first != parameter_list_range.second);

        static regex const comma_regex(",");
        sregex_token_iterator sub_arg_slider(parameter_list_range.first + 1, parameter_list_range.second - 1,
                comma_regex,
                -1);
        sregex_token_iterator sub_arg_end;
        //只提取出typename 和 class 后面的word, 其余的并不重要
        //TODO 待测试
        static regex const key_regex("(?:(?:typename)|(?:class))(?:\\.{3})?\\s+(\\w+)\\s*(=.*)?$");
        smatch type_match;
//        string sub_sentence;
        while (sub_arg_slider != sub_arg_end)
        {
//            sub_sentence = sub_arg_slider->str();
            if (regex_search(sub_arg_slider->first, sub_arg_slider->second, type_match, key_regex))
            {
                template_typename_set.insert(type_match[1].str());
            }
            ++sub_arg_slider;
        }
        key_search_begin = parameter_list_range.second;
        if (regex_search(key_search_begin, key_search_end, punct_match, punct_regex))
        {
            key_search_end = punct_match[0].first;
        }
    }
    
    wd_slider = sregex_iterator(key_search_begin, key_search_end, wd_regex);
    smatch keyword_match;

FINDKEY:
    while (wd_slider != siter_end)
    {
        if (IsKeyWord(*wd_slider))
        {
            keyword_match = *wd_slider;
            break;
        }
        ++wd_slider;
    }

    std::shared_ptr<ICodeElement> element;
    string key_str_name(keyword_match[0].str());
    if (key_str_name == "namespace")
    {
        // namespace匹配
        //如果是;结尾，表示是声明，不是定义
        if (break_character == ';')
        {
            ++(*fragment_slider_ptr);
            return nullptr;
        }
        element.reset(new HNamespaceElement(break_character));
    }
    else if (key_str_name == "using")
    {
        //如果是;结尾，表示是声明，不是定义
        if (break_character == ';')
        {
            ++(*fragment_slider_ptr);
            return nullptr;
        }
        else
        {
            ErrorExit("using not exit with\';\'in \"%s\"", sentence.c_str());
        }
    }
    else if (key_str_name == "typedef")
    {
        //如果typedef 后是一个结构，那么就按照那个结构来处理
        //这样做尽管不严谨，但是因为源代码中出现的不多，而且和类的继承没有直接关系，所以就这样吧
        if (break_character == '}')
        {
            ++wd_slider;
            goto FINDKEY;
        }
        element.reset(new HTypedefElement(break_character));
    }
    else if (key_str_name == "enum")
    {
        //如果是;结尾，表示是声明，不是定义
        if (break_character == ';')
        {
            ++(*fragment_slider_ptr);
            return nullptr;
        }
        ++wd_slider;
        if (wd_slider != siter_end && wd_slider->str() == "class")
        {
            element.reset(new HEnumElement(HEnumElement::EnumType::_ENUM_CLASS_, break_character));
        }
        else
        {
            element.reset(new HEnumElement(HEnumElement::EnumType::_ENUM_, break_character));
        }
    }
    else if (key_str_name == "class")
    {
        //如果是;结尾，表示是声明，不是定义
        if (break_character == ';')
        {
            ++(*fragment_slider_ptr);
            return nullptr;
        }
        element.reset(new HClassElement(HClassElement::ClassType::_CLASS_, is_template, std::move(template_typename_set), break_character));
    }
    else if (key_str_name == "union")
    {
        //如果是;结尾，表示是声明，不是定义
        if (break_character == ';')
        {
            ++(*fragment_slider_ptr);
            return nullptr;
        }
        element.reset(new HClassElement(HClassElement::ClassType::_UNION_, is_template, std::move(template_typename_set), break_character));
    }
    else if (key_str_name == "struct")
    {
        //如果是;结尾，表示是声明，不是定义
        if (break_character == ';')
        {
            ++(*fragment_slider_ptr);
            return nullptr;
        }
        element.reset(new HClassElement(HClassElement::ClassType::_STRUCT_, is_template, std::move(template_typename_set), break_character));
    }
    else
    {
        //处理函数，变量，friend声明等
        //如果是;结尾，表示是声明，不是定义
        if (break_character == ';')
        {
            ++(*fragment_slider_ptr);
            return nullptr;
        }
        element.reset(new HFunctionAndVariable(break_character));
    }


    if (element != nullptr)
    {
        //用以提取出名字等关键元素
        //对于函数和变量，不进行提取
        element->AnalyseWords(std::string(key_search_begin, sentence.cend()));
    } 
    return element;
}


/*
 *  ICodeElement
 *  用于记录退出状态
 */
//TODO 未测试
bool ICodeElement::BreakAnalyse(cregex_token_iterator* fragment_slider_ptr)
{ 
    assert(fragment_slider_ptr != nullptr);

    static regex const nunspace_reg("[^[:space:]]");
    static cregex_token_iterator const fragment_end;
    smatch nunspace_match;
    //用来将为空的片段过滤掉
    //string sentence = (*fragment_slider_ptr)->str();
    while (*fragment_slider_ptr != fragment_end && 
            !regex_search((*fragment_slider_ptr)->str(), nunspace_match, nunspace_reg))
    {
        ++(*fragment_slider_ptr);
     //   sentence = (*fragment_slider_ptr)->str();
    }

    //根据算法，不会遇到片段结束的情况，除了filecontact类
    //assert(*fragment_slider_ptr != fragment_end);
    if (*fragment_slider_ptr == fragment_end)
    {
        //this->PrintName();
        cerr<< "*fragment_slider_ptr == fragment_end" << endl;
        assert(false);
    }

    char character = nunspace_match[0].str().at(0);
    //如果片段中第一个非空字符和对象自带的break_character_一样，那表示到了结束点，返回true
    bool ret = (character == break_character_);
    //如果片段中包含结束标识符，那么指向下一个片段
    if (character == ';' || character == '}')
        ++(*fragment_slider_ptr);
    return ret; 
}

//TODO 未测试
void ICodeElement::AnalyseSubCode(cregex_token_iterator* fragment_slider_ptr)
{
    assert(fragment_slider_ptr != nullptr);
    shared_ptr<ICodeElement> sub_element;
    //string sentence = (*fragment_slider_ptr)->str();
    while (!BreakAnalyse(fragment_slider_ptr))
    {
        //通过Createelementfactory来获取子元素
        //sentence = (*fragment_slider_ptr)->str();
        sub_element = CreateElementFactory(fragment_slider_ptr);
        if (sub_element != nullptr)
        {
            //分析子元素的的代码段
            sub_element->AnalyseSubCode(fragment_slider_ptr);
            //将子元素的内容进行添加
            AddSubElement(sub_element);
        }
    }
}


//TODO 未测试
void ICodeElement::AddSubElement(std::shared_ptr<ICodeElement> subele)
{
    //对于非匿名的元素进行添加
    if (subele->GetElementName().length() > 0)
    {
        sub_elements_.push_back(subele);
    }
}
/*
 * HFileContents
 */
//TESTED~ 
void HFileContents::AnalyseWords(std::string const& sentence)
{

    full_path_.assign(sentence);

    static regex const flags_regex("/((Classes)|(Private)|(Public)|(Resources))/");
    smatch flag_match;
    if (regex_search(sentence, flag_match, flags_regex))
    {
        switch (flag_match.str().at(2))
        {
            case 'l':
                {
                    path_flag_ = PathFlags::_CLASSES_;
                    break;
                }
            case 'r':
                {
                    path_flag_ = PathFlags::_PRIVATE_;
                    break;
                }
            case 'u':
                {
                    path_flag_ = PathFlags::_PUBLIC_;
                    break;
                }
            case 'e':
                {
                    path_flag_ = PathFlags::_RESOURCES_;
                    break;
                }
        }

        package_name_.assign(sentence.cbegin(), flag_match[0].first);
        identity_name_.assign(flag_match.suffix().str());
    }
    else
    {
        path_flag_ = PathFlags::_UNDEFINED_;
        static regex const split_regex("/([[:w:].]+)\\s*$");
        smatch identity_match;

        assert(regex_search(sentence, identity_match, split_regex));

        package_name_.assign(sentence.cbegin(), identity_match[0].first);
        identity_name_.assign(identity_match[1].str());
    }
}
//Tested~~
bool HFileContents::BreakAnalyse(cregex_token_iterator* fragment_slider_ptr)
{ 
    assert(fragment_slider_ptr != nullptr);

    static cregex_token_iterator const fragment_end;

    static regex const nunspace_reg("[^[:space:]]");
    smatch nunspace_match;
    //用来将为空的片段过滤掉
    while (*fragment_slider_ptr != fragment_end && 
            !regex_search((*fragment_slider_ptr)->str(), nunspace_match, nunspace_reg))
    {
        ++(*fragment_slider_ptr);
    }
    //如果文件片段指针遍历完了，那么就表示结束了
    if (*fragment_slider_ptr == fragment_end)
    {
        return true;
    }

    char character = nunspace_match[0].str().at(0);
    //如果片段中包含结束标识符，那么指向下一个片段，去除文件中多余的";"号
    if (character == ';')
        ++(*fragment_slider_ptr);
    return false; 
}
/*
 *  HNamespaceElement
 */

//Tested~~
void HNamespaceElement::AnalyseWords(std::string const& sentence)
{
    regex key_regex("namespace(?:\\s+(\\w+))?\\s*$");

    smatch key_match;

    assert(regex_search(sentence, key_match, key_regex));

    if (key_match[1].str().empty())
    {
        anonymity_ = true;
    }
    else
    {
        anonymity_ = false;
        name_ = key_match[1].str();
    }
}

/*
   void HNamespaceElement::AddSubElement(std::shared_ptr<ICodeElement> subele)
   {

   }
   */

/*
 *  HClassElement
 */
//Tested~~~
void HClassElement::AnalyseWords(std::string const& sentence)
{


    //用以匹配非数字开头的word
    static regex const wd_regex("[a-zA-Z_]\\w*");
    //如果有继承, 那么分离出继承
    static regex const inher_regex("[:]\\s*(?:(?:public)|(?:private)|(?:protected))");
    smatch inher_match;
    sregex_iterator wd_slider;
    sregex_iterator siter_end;
    auto str_cbegin = sentence.cbegin();
    auto str_cend = sentence.cend();
    if (regex_search(str_cbegin, str_cend, inher_match, inher_regex)) 
    {
        //先将<>中的元素进行处理
        //先拷贝
        std::string inherent_str_copy(inher_match[0].first, str_cend);
        //将第一个:改成,方便查找
        inherent_str_copy[0] = ',';
        std::pair<std::string::const_iterator, std::string::const_iterator> bracket_range;
        bracket_range = FindBracketRange<'<','>'> (inherent_str_copy.cbegin(), inherent_str_copy.cend());
        while (bracket_range.first != bracket_range.second)
        {
            wd_slider = sregex_iterator(bracket_range.first, bracket_range.second, wd_regex);
            while(wd_slider != siter_end)
            {
                if (!Ignored(*wd_slider))
                {
                    associate_class_set_.insert(wd_slider->str());
                }
                ++wd_slider;
            }
            inherent_str_copy.replace(bracket_range.first, bracket_range.second, ' ', 1);
            bracket_range = FindBracketRange<'<','>'> (inherent_str_copy.cbegin(), inherent_str_copy.cend());
        }

        //对继承关系进行处理
        static regex const p_key_regex("[,]([^,]*)"); 
        sregex_iterator base_class_slider(inherent_str_copy.cbegin(), inherent_str_copy.cend(), p_key_regex);
        smatch base_str_match;
        while (base_class_slider != siter_end)
        {
            base_str_match = *base_class_slider;
            wd_slider = sregex_iterator(base_str_match[1].first, base_str_match[1].second, wd_regex);
            smatch base_match;
            while (wd_slider != siter_end)
            {
                if (!Ignored(*wd_slider))
                {
                    base_match = *wd_slider;
                }
                ++wd_slider;
            }
            if (base_match.length() > 0)
                base_class_set_.insert(base_match.str());
            ++base_class_slider;
        }
        str_cend = inher_match[0].first;
    }

    std::string key_str;
    switch (this->type_)
    {
        case _CLASS_:
            key_str = "class";
            break;
        case _STRUCT_:
            key_str = "struct";
            break;
        case _UNION_:
            key_str = "union";
            break;
    }

    regex class_name_regex(key_str +"\\s+(\\w+[[:w:][:s:]:]*)(?:\\s*<(.*)>\\s*)?");
//    //类名前有可能有用来描述类的宏,对于模板类来说，它可能是某个类的特例化，因此会有"<.*>"出现
//    regex class_name_regex(key_str + "(?:\\s+(?:\\w+))?\\s+(\\w+)(?:\\s*<(.*)>\\s*)?");
    smatch class_name_match;
    //检查性约束
    if (!regex_search(str_cbegin, str_cend, class_name_match, class_name_regex))
    {
        class_name_regex = regex(key_str + "\\s*$");
        if (regex_search(str_cbegin, str_cend, class_name_regex))
        {
            anonymity_ = true;
            name_.clear();
        }
        else
        {
            //TODO 可以删除
            while (str_cbegin != str_cend && *str_cbegin == ' ')
                ++str_cbegin;
            cerr<< sentence<< endl;
            ErrorExit("Class attrieve name error: not find match in string: \"%s\"", string(str_cbegin, str_cend).c_str());
        }
    }
    else            // if (!class_name_match[1].str().empty())
    {
        anonymity_ = false;

        static regex const name_regex("(\\w+)\\s*$");
        smatch name_match;
        assert(regex_search(class_name_match[1].first, class_name_match[1].second, name_match, name_regex));
        name_.assign(name_match[1].str());
        if (!class_name_match[2].str().empty())
        {
            sregex_iterator wd_slider(class_name_match[2].first, class_name_match[2].second,
                    wd_regex);
            sregex_iterator wd_end;
            while(wd_slider != wd_end)
            {
                if (!Ignored(*wd_slider))
                {
                    associate_class_set_.insert(wd_slider->str());
                }
                ++wd_slider;
            }
        }
    }
}

/*
   void HClassElement::AddSubElement(std::shared_ptr<ICodeElement> subele)
   {

   }
   */

/*
 * HEnumElement 
 *
 */

//Tested~~~
void HEnumElement::AnalyseWords(std::string const& sentence)
{
    //用以匹配非数字开头的word
    static regex const wd_regex("[a-zA-Z_]\\w*");
    std::string key_str;
    switch (this->type_)
    {
        case _ENUM_:
            key_str = "enum";
            break;
        case _ENUM_CLASS_:
            key_str = "enum\\s+class";
            break;
    }

    regex enum_name_regex(key_str + "(?:\\s+(?:\\w+))?\\s+(\\w+)");
    smatch enum_name_match;
    if (!regex_search(sentence.cbegin(), sentence.cend(), enum_name_match, enum_name_regex))
    {
        anonymity_ = true;
        name_.clear();
    }
    else     
    {
        anonymity_ = false;
        name_.assign(enum_name_match[1].str());
    }
}



//TODO 未测试
void HEnumElement::AnalyseSubCode(cregex_token_iterator* fragment_slider_ptr)
{
    assert(fragment_slider_ptr != nullptr);
    static regex const wd_regex("\\w+");
    static cregex_iterator const wd_end;
    static cregex_iterator wd_slider;
    while (!BreakAnalyse(fragment_slider_ptr))
    {
        wd_slider = cregex_iterator((*fragment_slider_ptr)->first, (*fragment_slider_ptr)->second, wd_regex);
        while (wd_slider != wd_end)
        {
            if (!Ignored(*wd_slider))
                enum_element_set.insert((*wd_slider)[0].str());
            ++wd_slider;
        }
        ++(*fragment_slider_ptr);
    }
}

//Tested~
//如果是函数，那么返回false
//如果是变量，那么返回ture
bool HTypedefElement::CutVariable(std::string const& sentence)
{
    //TODO 
    std::string* name_ptr = &name_;
    std::string* namespace_ptr = &namespace_;
    std::string* type_ptr = &type_name_;
    std::set<std::string>* associate_vec_ptr = &associate_class_set_;

    //去除关键字typename template typedef class const constexpr union struct inline static public private return 
    //FORCEINLINE 等待这需要预先进行处理
    //对operator另外处理

    //先对auto ->这样的函数进行忽略处理
    static regex const auto_arrow_regex("auto\\s+(.*)->(?:.*)$");
    smatch auto_arrow_match; 


    if (regex_search(sentence, auto_arrow_match, auto_arrow_regex))
    {
        return false;
    }

    //对于auto = 不进行处理
    static regex const auto_equal_regex("auto .*=.*");
    if (regex_search(sentence, auto_equal_regex))
    {
        //TODO 
        cerr<< "!!!! CutFunctionOrVariable auto .*=.* find:\""<< sentence<< "\"\n";
        return false;
    }

    static regex const wd_regex("\\w+");

    //
    /*
     * 先分成两类,一类包含括号,另一类不包含括号
     */

    /*
     * 包含括号的为函数定义或者类型定义
     */
    //如果最后一个元素是word并且不是关键字,那么就是类型

    smatch name_match;
    std::sregex_iterator siter_end;
    std::sregex_iterator word_iter;

    static regex const parenthesis_regex("\\(.*\\)\\s*([^[:s:]\\(\\)]+)?\\s*$");
    smatch parenthesis_match;
    //带有括号的语句
    if (regex_search(sentence, parenthesis_match, parenthesis_regex))
    {
        //以括号结尾，是函数，返回
        if (parenthesis_match[1].str().empty())
        {
            return false;
        }
        //找到括号后最后一个不可忽略的word
        word_iter = std::sregex_iterator(parenthesis_match[1].first,parenthesis_match[1].second, wd_regex);
        while (word_iter != siter_end)
        {
            if (!Ignored(*word_iter))
            {
                name_match = *word_iter;
            }
            ++word_iter;
        }
        //如果不存在，那么就是函数，返回
        //否则是变量名
        if (name_match.empty())
        {
            return false;
        }
        else
        {
            *name_ptr = name_match.str();
        }
    }
    else
    {
        //不含有括号的变量
        //找到名字
        static regex const name_regex("(\\w+)(?:\\[[a-zA-Z0-9]*\\])?\\s*$");
        if (!regex_search(sentence, name_match, name_regex) || name_match[1].str().empty())
        {
            cerr<< "!!!! name_match error:\""<< sentence<< "\""<< endl; 
        }
        *name_ptr = name_match[1].str();
    }


    //用来处理尖括号<>中间的内容
    //提取出尖括号中的元素，如果不是忽略的，那么就加入到associate_vector集合中
    static std::regex const bracket_regex("<(.*)>");
    auto t_iter = sentence.cbegin();
    std::sregex_iterator bracket_iter(sentence.cbegin(), name_match[0].first, bracket_regex);
    smatch bracket_match;

    std::string sentence_copy;      //用于保存在尖括号之外的部分
    while (bracket_iter != siter_end)
    {
        bracket_match = *bracket_iter;

        word_iter = std::sregex_iterator(bracket_match[1].first, bracket_match[1].second, wd_regex);
        while (word_iter != siter_end)
        {
            if (!Ignored(*word_iter))
            {
                associate_vec_ptr->insert(word_iter->str());
            }
            ++word_iter;
        }

        sentence_copy.append(t_iter, bracket_match[0].first);
        t_iter = bracket_match[0].second;
        ++bracket_iter;
    }
    sentence_copy.append(t_iter, name_match[0].first);

    //从sentence_copy中提取出::之前的内容,作为命名空间
    //如果有连续命名空间，那么只保留最后一个
    static std::regex const namespace_regex("(\\w+)?\\s*::");
    std::sregex_iterator namespace_iter(sentence_copy.cbegin(), sentence_copy.cend(), namespace_regex);
    smatch namespace_match;
    while (namespace_iter != siter_end)
    {
        if (!Ignored(*namespace_iter))
            namespace_match = *namespace_iter;
        ++namespace_iter;
    }
    if (!namespace_match[1].str().empty())
    {
        *namespace_ptr = namespace_match[1].str();
    }
    //提取类型信息
    t_iter = sentence_copy.cbegin();
    //如果有命名空间，那么从命名空间后提取类型信息
    if (!namespace_match.empty())
    {
        t_iter = namespace_match[0].second;
    }
    word_iter = std::sregex_iterator(t_iter, sentence_copy.cend(), wd_regex);
    smatch type_match;
    while (word_iter != siter_end)
    {
        if (!Ignored(*word_iter))
        {
            type_match = *word_iter;
        }
        ++word_iter;
    }
    if (type_match.empty())
    {
        cerr<< "!!!! type_match error:\""<< sentence_copy<< "\""<< "\t\t\t"<<sentence << endl; 
    }
    *type_ptr = type_match.str();
    return true;
}

/*
 *  HTypedefElement
 */
//TODO 未测试
void HTypedefElement::AnalyseWords(std::string const& sentence)
{
    if (CutVariable(sentence))
    {
        anonymity_ = false;
    }
    else
    {
        anonymity_ = true;
    }
}
//TODO 未测试
void HTypedefElement::AnalyseSubCode(cregex_token_iterator* fragment_slider_ptr)
{
    assert(fragment_slider_ptr != nullptr);
    //对Typedef不进行任何子元素提取,直接找到结束处
    //TODO 保护性约束
    if (!BreakAnalyse(fragment_slider_ptr))
    {
        ErrorExit("TypedefElement AnalyseSubCode error, BreakAnalyse return false");
    }
}


/*
 *  HUsingElement
 void HUsingElement::AnalyseWords(std::string const& sentence)
 {
//不进行提取
return;
}
void HUsingElement::AnalyseSubCode(cregex_token_iterator* fragment_slider_ptr)
{
assert(fragment_slider_ptr != nullptr);
//对using不进行任何子元素提取,直接找到结束处
if (!BreakAnalyse(fragment_slider_ptr))
{
ErrorExit("UsingElement AnalyseSubCode error, BreakAnalyse return false");
}
}
*/

/*
 *  HFunctionAndVariable
 */
//TODO 未测试
bool HFunctionAndVariable::BreakAnalyse(cregex_token_iterator* fragment_slider_ptr)
{ 
    assert(fragment_slider_ptr != nullptr);

    regex brace_semicolon_reg("[\\{\\};]");
    static cregex_token_iterator const fragment_end;
    smatch brace_semicolon_match;
    char character;
    do
    {
        //用来将代码过滤掉
        while (*fragment_slider_ptr != fragment_end && 
                !regex_search((*fragment_slider_ptr)->str(), brace_semicolon_match, brace_semicolon_reg))
        {
            ++(*fragment_slider_ptr);
        }

        assert(*fragment_slider_ptr != fragment_end);

        character = brace_semicolon_match[0].str().at(0);
        switch (character)
        {
            case '{':
                {
                    ++left_brace_count_;
                    break;
                }
            case '}':
                --left_brace_count_;
            case ';':
                if (left_brace_count_ == 0 && character == break_character_)
                {
                    ++(*fragment_slider_ptr);
                    return true;
                }
        }
        ++(*fragment_slider_ptr);
    } while (1);
}

//TODO 未测试
void HFunctionAndVariable::AnalyseSubCode(cregex_token_iterator* fragment_slider_ptr)
{
    assert(fragment_slider_ptr != nullptr);
    //对函数和变量不进行任何子元素提取,直接找到结束处
    BreakAnalyse(fragment_slider_ptr);
}

//TODO 未测试
void HFunctionAndVariable::AnalyseWords(std::string const& sentence)
{
    //太难，不进行提取
    anonymity_ = true;
    return;
}




/*
 *  打印，用于测试
 */
//TODO 未测试
void ICodeElement::PrintForTest()
{
    PrintName();
    PrintSubElement();
}

void ICodeElement::PrintSubElement()
{
    if (sub_elements_.empty())
    {
        cout<< ";"<< endl;
        return;
    }
    cout<< "{"<< endl;
    for (auto ele: sub_elements_)
    {
        ele->PrintForTest();
    }
    cout<< "}"<< endl;
}

void HFileContents::PrintName()
{
    cout<< "//package_name_://"<< package_name_<< "\t"<< "//identity_name_://"<< identity_name_<< endl;
}
void HNamespaceElement::PrintName()
{
    cout<< "namespace "<< name_<< endl;
}
void HClassElement::PrintName()
{
    if (template_)
    {
        cout<< "/*template "<< "<";
        for (auto str: template_typename_set_)
        {
            cout<<str<< ", ";
        }
        cout<< ">*/"<< endl;
    }
    cout<< "class "<< name_;
    if (!base_class_set_.empty())
    {
        cout<< ":";
        for (auto str: base_class_set_)
        {
            cout<< str<< ", ";
        }
    }
    if (!associate_class_set_.empty())
    {
        cout<< endl;
        cout<< "(";
        for (auto str: associate_class_set_)
        {
            cout<< str<< ", ";
        }
        cout<< ")";
    }
    cout<< endl;
}

void HEnumElement::PrintName()
{
    cout<< "enum"<< name_<< endl;
    cout<< "{";
    for (auto ele: enum_element_set)
    {
        cout<< ele<< ", ";
    }
    cout<< "}";
}
void HTypedefElement::PrintName()
{
    cout<< "typedef "<< name_<< "->[";
    if (!namespace_.empty())
    {
        cout<< namespace_<< "::";
    }
    if (!type_name_.empty())
    {
        cout<< type_name_;
    }
    cout<< "]";
    if (!associate_class_set_.empty())
    {
        cout<< "\t(";
        for (auto str: associate_class_set_)
        {
            cout<< str<< ", ";
        }
        cout<< ")";
    }
}
void HFunctionAndVariable::PrintName()
{
    return ;
}
void HFileContents::PrintFile(std::cregex_token_iterator* fragment_slider_ptr)
{
    static std::cregex_token_iterator const citer_end;
    while (*fragment_slider_ptr != citer_end)
    {
        cout<< "\""<< (*fragment_slider_ptr)->str()<< "\""<< endl;
        ++(*fragment_slider_ptr);
    }
}



