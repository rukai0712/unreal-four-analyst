#pragma once

#include<iostream>
#include<regex>
#include<vector>
#include<string>
#include<set>

namespace RKHT{

    /*
     *  公共接口，来代表一个基本的元素
     */
    class ICodeElement
    {
        public:
//            ICodeElement()
//            {}
            explicit ICodeElement(char bk = ';')
                :break_character_(bk)
            {}

            virtual ~ICodeElement()
            {}


            virtual void AnalyseSubCode(std::cregex_token_iterator* fragment_slider_ptr);
            virtual void AddSubElement(std::shared_ptr<ICodeElement> subele);
            //可以获得元素的名称等信息
            virtual void AnalyseWords(std::string const& sentence) = 0;
            std::string const& GetElementName(void) const { return name_; }

            //用于测试
            virtual void PrintForTest(void);

            //用来标志当前代码段的结束
            void SetBreakCharacter(char bk)                                 
            { 
                break_character_ = bk; 
            } 
        protected:
            //用于确定当前的片段是否有SubCode分析结束的标记
            virtual bool BreakAnalyse(std::cregex_token_iterator* fragment_slider_ptr) ;

            //用于测试
            virtual void PrintName() = 0;
            virtual void PrintSubElement();
        protected:
            //默认以;结尾表示结束
            char break_character_;
            //元素是否是匿名的
            bool anonymity_ = true;
            std::string name_;
            //用来保存它包含的子元素
            std::vector<std::shared_ptr<ICodeElement>> sub_elements_;
    };

    /*
     *  文件容器，用于容纳文件中的元素
     */
    class HFileContents:public ICodeElement
    {
        public:
            enum PathFlags
            {
                _CLASSES_,
                _PRIVATE_,
                _PUBLIC_,
                _RESOURCES_,
                _UNDEFINED_,
            };
        public:
            HFileContents()
            {}
            virtual ~HFileContents()
            {}

            //virtual void AddSubElement(std::shared_ptr<ICodeElement> subele) override;
            //根据文件全面来获取文件信息
            virtual void AnalyseWords(std::string const& sentence) override;

            void PrintFile(std::cregex_token_iterator* fragment_slider_ptr);
        private:
            //用于确定当前的片段是否有SubCode分析结束的标记
            virtual bool BreakAnalyse(std::cregex_token_iterator* fragment_slider_ptr) override;

            //用于测试
            virtual void PrintName() override;
        private:
            PathFlags  path_flag_;
            //路径全名
            std::string full_path_;
            //将classed,private,public,resources等目录的上一级目录名作为package_name_;
            std::string package_name_;
            //将classed,private,public,resources等目录的下一级目录开始包括文件名一起作为identity_name_;
            std::string identity_name_;
    }; 
    /*
     *  命名空间元素
     */
    class HNamespaceElement:public ICodeElement
    {
        public:
            explicit HNamespaceElement(char bk = ';')
                :ICodeElement(bk)
            {}
            virtual ~HNamespaceElement()
            {}

            //用于分析namespace的名字
            virtual void AnalyseWords(std::string const& sentence) override;
            //virtual void AddSubElement(std::shared_ptr<ICodeElement> subele) override;
        private:
            //用于测试
            virtual void PrintName() override;
    };

    /*
     *  集合元素，包括类，枚举，聚合
     */
    class HClassElement:public ICodeElement
    {
        public:
            enum ClassType{
                _CLASS_,
                _STRUCT_, 
                _UNION_,
            };
        public:
            explicit HClassElement(char bk = ';')
                :ICodeElement(bk)
            {}
            virtual ~HClassElement()
            {}

            explicit HClassElement(ClassType type, bool is_template, std::set<std::string>&& template_typename_set, char bk = ';')
                :ICodeElement(bk), 
                type_(type), 
                template_(is_template), 
                template_typename_set_(std::move(template_typename_set)) 
            {}
            virtual void AnalyseWords(std::string const& sentence) override;
            //virtual void AddSubElement(std::shared_ptr<ICodeElement> subele) override;
        private:
            //用于测试
            virtual void PrintName() override;

        private:
            ClassType type_;
            bool template_ = false;
            //用以放置模板参数列表
            std::set<std::string> template_typename_set_;
            //放置继继承的类
            std::set<std::string> base_class_set_;
            //放置特例化的参数
            std::set<std::string> associate_class_set_;
    };

    class HEnumElement:public ICodeElement
    {
        public:
            enum EnumType{
                _ENUM_,
                _ENUM_CLASS_,
            };
        public:
             explicit HEnumElement(char bk = ';')
                :ICodeElement(bk)
            {}
            virtual ~HEnumElement()
            {}

            explicit HEnumElement(EnumType type, char bk = ';')
                :ICodeElement(bk), 
                type_(type) 
            {}

            virtual void AnalyseSubCode(std::cregex_token_iterator* fragment_slider_ptr) override;
            virtual void AnalyseWords(std::string const& sentence) override;
            //virtual void AddSubElement(std::shared_ptr<ICodeElement> subele) override;
        private:
            //用于测试
            virtual void PrintName() override;
        private:
            EnumType type_;
            std::set<std::string> enum_element_set;
    };


    class HTypedefElement:public ICodeElement
    {
        public:
            explicit HTypedefElement(char bk = ';')
                :ICodeElement(bk)
            {}
            virtual ~HTypedefElement()
            {}

            virtual void AnalyseWords(std::string const& sentence) override;
            virtual void AnalyseSubCode(std::cregex_token_iterator* fragment_slider_ptr) override;
            virtual void AddSubElement(std::shared_ptr<ICodeElement> subele) override
            {
                return;
            }
        private:
            //用于测试
            virtual void PrintName() override;
        private:
            bool CutVariable(std::string const& sentence);
            //bool variable_;
            std::string namespace_;
            std::string type_name_;
            //放置特例化的参数
            std::set<std::string> associate_class_set_;
    };


    /*
     * using定义
     class HUsingElement:public ICodeElement
     {
     public:
     HUsingElement()
     {}
     virtual ~HUsingElement()
     {}

     virtual void AnalyseWords(std::string const& sentence) override;
     virtual void AnalyseSubCode(cregex_token_iterator* fragment_slider_ptr) override;
     virtual void AddSubElement(std::shared_ptr<ICodeElement> subele) override
     {
     return;
     }

     private:

     };
     */

    /*
     *  函数或变量元素
     */
    class HFunctionAndVariable:public ICodeElement
    {
        public:
            explicit HFunctionAndVariable(char bk = ';')
                :ICodeElement(bk)
            {
                if (bk == '}')
                    left_brace_count_ = 1;
            }
            virtual ~HFunctionAndVariable()
            {}


            //用来分析函数或变量声明的部分
            virtual void AnalyseWords(std::string const& sentence) override;
            //函数体内部进行元素分析
            virtual void AnalyseSubCode(std::cregex_token_iterator* fragment_slider_ptr) override;
            //为空,因为函数和变量内部的子元素不提取
            virtual void AddSubElement(std::shared_ptr<ICodeElement> subele) override
            {
                return ;
            }
        private:
            //用于确定当前的片段是否有SubCode分析结束的标记
            virtual bool BreakAnalyse(std::cregex_token_iterator* fragment_slider_ptr) override;
            //用于测试
            virtual void PrintName() override;
        private:
            //用以标记代码中左括号出现的次数
            int left_brace_count_ = 0;
    };

}
