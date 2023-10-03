/*
 * Copyright 2015-2017 Kevin Wojniak
 *
 * Permission is hereby granted, free of charge, to any person or organization
 * obtaining a copy of the software and accompanying documentation covered by
 * this license (the "Software") to use, reproduce, display, distribute,
 * execute, and transmit the Software, and to prepare derivative works of the
 * Software, and to permit third-parties to whom the Software is furnished to
 * do so, all subject to the following:
 *
 * The copyright notices in the Software and this entire statement, including
 * the above license grant, this restriction and the following disclaimer,
 * must be included in all copies of the Software, in whole or in part, and
 * all derivative works of the Software, unless such copies or derivative
 * works are solely in the form of machine-executable object code generated by
 * a source language processor.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
 * SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
 * FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef KAINJOW_MUSTACHE_HPP
#define KAINJOW_MUSTACHE_HPP

#include <cassert>
#include <functional>
#include <iostream>
#include <memory>
#include <sstream>
#include <unordered_map>
#include <vector>

namespace kainjow {
namespace mustache {

template <typename string_type>
string_type trim(const string_type& s) {
    auto it = s.begin();
    while (it != s.end() && isspace(*it)) {
        it++;
    }
    auto rit = s.rbegin();
    while (rit.base() != it && isspace(*rit)) {
        rit++;
    }
    return {it, rit.base()};
}

template <typename string_type>
string_type html_escape(const string_type& s) {
    string_type ret;
    ret.reserve(s.size()*2);
    for (const auto ch : s) {
        switch (ch) {
            case '&':
                ret.append({'&','a','m','p',';'});
                break;
            case '<':
                ret.append({'&','l','t',';'});
                break;
            case '>':
                ret.append({'&','g','t',';'});
                break;
            case '\"':
                ret.append({'&','q','u','o','t',';'});
                break;
            case '\'':
                ret.append({'&','a','p','o','s',';'});
                break;
            default:
                ret.append(1, ch);
                break;
        }
    }
    return ret;
}

template <typename string_type>
std::vector<string_type> split(const string_type& s, typename string_type::value_type delim) {
    std::vector<string_type> elems;
    std::basic_stringstream<typename string_type::value_type> ss(s);
    string_type item;
    while (std::getline(ss, item, delim)) {
        elems.push_back(item);
    }
    return elems;
}

template <typename string_type>
class basic_renderer {
public:
    using type1 = std::function<string_type(const string_type&)>;
    using type2 = std::function<string_type(const string_type&, bool escaped)>;

    string_type operator()(const string_type& text) const {
        return type1_(text);
    }

    string_type operator()(const string_type& text, bool escaped) const {
        return type2_(text, escaped);
    }

private:
    basic_renderer(const type1& t1, const type2& t2)
        : type1_(t1)
        , type2_(t2)
    {}

    const type1& type1_;
    const type2& type2_;

    template <typename StringType>
    friend class basic_mustache;
};

template <typename string_type>
class basic_lambda_t {
public:
    using type1 = std::function<string_type(const string_type&)>;
    using type2 = std::function<string_type(const string_type&, const basic_renderer<string_type>& render)>;

    basic_lambda_t(const type1& t) : type1_(new type1(t)) {}
    basic_lambda_t(const type2& t) : type2_(new type2(t)) {}

    bool is_type1() const { return static_cast<bool>(type1_); }
    bool is_type2() const { return static_cast<bool>(type2_); }

    const type1& type1_value() const { return *type1_; }
    const type2& type2_value() const { return *type2_; }

    // Copying
    basic_lambda_t(const basic_lambda_t& l) {
        if (l.type1_) {
            type1_.reset(new type1(*l.type1_));
        } else if (l.type2_) {
            type2_.reset(new type2(*l.type2_));
        }
    }

    string_type operator()(const string_type& text) const {
        return (*type1_)(text);
    }

    string_type operator()(const string_type& text, const basic_renderer<string_type>& render) const {
        return (*type2_)(text, render);
    }

private:
    std::unique_ptr<type1> type1_;
    std::unique_ptr<type2> type2_;
};

template <typename string_type>
class basic_data;
template <typename string_type>
using basic_object = std::unordered_map<string_type, basic_data<string_type>>;
template <typename string_type>
using basic_list = std::vector<basic_data<string_type>>;
template <typename string_type>
using basic_partial = std::function<string_type()>;
template <typename string_type>
using basic_lambda = typename basic_lambda_t<string_type>::type1;
template <typename string_type>
using basic_lambda2 = typename basic_lambda_t<string_type>::type2;

template <typename string_type>
class basic_data {
public:
    enum class type {
        object,
        string,
        list,
        bool_true,
        bool_false,
        partial,
        lambda,
        lambda2,
        invalid,
    };

    // Construction
    basic_data() : basic_data(type::object) {
    }
    basic_data(const string_type& string) : type_{type::string} {
        str_.reset(new string_type(string));
    }
    basic_data(const typename string_type::value_type* string) : type_{type::string} {
        str_.reset(new string_type(string));
    }
    basic_data(const basic_object<string_type>& obj) : type_{type::object} {
        obj_.reset(new basic_object<string_type>(obj));
    }
    basic_data(const basic_list<string_type>& l) : type_{type::list} {
        list_.reset(new basic_list<string_type>(l));
    }
    basic_data(type t) : type_{t} {
        switch (type_) {
            case type::object:
                obj_.reset(new basic_object<string_type>);
                break;
            case type::string:
                str_.reset(new string_type);
                break;
            case type::list:
                list_.reset(new basic_list<string_type>);
                break;
            default:
                break;
        }
    }
    basic_data(const string_type& name, const basic_data& var) : basic_data{} {
        set(name, var);
    }
    basic_data(const basic_partial<string_type>& p) : type_{type::partial} {
        partial_.reset(new basic_partial<string_type>(p));
    }
    basic_data(const basic_lambda<string_type>& l) : type_{type::lambda} {
        lambda_.reset(new basic_lambda_t<string_type>(l));
    }
    basic_data(const basic_lambda2<string_type>& l) : type_{type::lambda2} {
        lambda_.reset(new basic_lambda_t<string_type>(l));
    }
    basic_data(const basic_lambda_t<string_type>& l) {
        if (l.is_type1()) {
            type_ = type::lambda;
        } else if (l.is_type2()) {
            type_ = type::lambda2;
        }
        lambda_.reset(new basic_lambda_t<string_type>(l));
    }
    basic_data(bool b) : type_{b ? type::bool_true : type::bool_false} {
    }

    // Copying
    basic_data(const basic_data& dat) : type_(dat.type_) {
        if (dat.obj_) {
            obj_.reset(new basic_object<string_type>(*dat.obj_));
        } else if (dat.str_) {
            str_.reset(new string_type(*dat.str_));
        } else if (dat.list_) {
            list_.reset(new basic_list<string_type>(*dat.list_));
        } else if (dat.partial_) {
            partial_.reset(new basic_partial<string_type>(*dat.partial_));
        } else if (dat.lambda_) {
            lambda_.reset(new basic_lambda_t<string_type>(*dat.lambda_));
        }
    }

    // Move
    basic_data(basic_data&& dat) : type_{dat.type_} {
        if (dat.obj_) {
            obj_ = std::move(dat.obj_);
        } else if (dat.str_) {
            str_ = std::move(dat.str_);
        } else if (dat.list_) {
            list_ = std::move(dat.list_);
        } else if (dat.partial_) {
            partial_ = std::move(dat.partial_);
        } else if (dat.lambda_) {
            lambda_ = std::move(dat.lambda_);
        }
        dat.type_ = type::invalid;
    }
    basic_data& operator= (basic_data&& dat) {
        if (this != &dat) {
            obj_.reset();
            str_.reset();
            list_.reset();
            partial_.reset();
            lambda_.reset();
            if (dat.obj_) {
                obj_ = std::move(dat.obj_);
            } else if (dat.str_) {
                str_ = std::move(dat.str_);
            } else if (dat.list_) {
                list_ = std::move(dat.list_);
            } else if (dat.partial_) {
                partial_ = std::move(dat.partial_);
            } else if (dat.lambda_) {
                lambda_ = std::move(dat.lambda_);
            }
            type_ = dat.type_;
            dat.type_ = type::invalid;
        }
        return *this;
    }

    // Type info
    bool is_object() const {
        return type_ == type::object;
    }
    bool is_string() const {
        return type_ == type::string;
    }
    bool is_list() const {
        return type_ == type::list;
    }
    bool is_bool() const {
        return is_true() || is_false();
    }
    bool is_true() const {
        return type_ == type::bool_true;
    }
    bool is_false() const {
        return type_ == type::bool_false;
    }
    bool is_partial() const {
        return type_ == type::partial;
    }
    bool is_lambda() const {
        return type_ == type::lambda;
    }
    bool is_lambda2() const {
        return type_ == type::lambda2;
    }
    bool is_invalid() const {
        return type_ == type::invalid;
    }

    // Object data
    void set(const string_type& name, const basic_data& var) {
        if (is_object()) {
            obj_->insert(std::pair<string_type,basic_data>{name, var});
        }
    }
    const basic_data* get(const string_type& name) const {
        if (!is_object()) {
            return nullptr;
        }
        const auto& it = obj_->find(name);
        if (it == obj_->end()) {
            return nullptr;
        }
        return &it->second;
    }

    // List data
    void push_back(const basic_data& var) {
        if (is_list()) {
            list_->push_back(var);
        }
    }
    const basic_list<string_type>& list_value() const {
        return *list_;
    }
    bool is_empty_list() const {
        return is_list() && list_->empty();
    }
    bool is_non_empty_list() const {
        return is_list() && !list_->empty();
    }
    basic_data& operator<< (const basic_data& data) {
        push_back(data);
        return *this;
    }

    // String data
    const string_type& string_value() const {
        return *str_;
    }

    basic_data& operator[] (const string_type& key) {
        return (*obj_)[key];
    }

    const basic_partial<string_type>& partial_value() const {
        return (*partial_);
    }

    const basic_lambda<string_type>& lambda_value() const {
        return lambda_->type1_value();
    }

    const basic_lambda2<string_type>& lambda2_value() const {
        return lambda_->type2_value();
    }

private:
    type type_;
    std::unique_ptr<basic_object<string_type>> obj_;
    std::unique_ptr<string_type> str_;
    std::unique_ptr<basic_list<string_type>> list_;
    std::unique_ptr<basic_partial<string_type>> partial_;
    std::unique_ptr<basic_lambda_t<string_type>> lambda_;
};

template <typename string_type>
class delimiter_set {
public:
    string_type begin;
    string_type end;
    delimiter_set()
        : begin(default_begin)
        , end(default_end)
    {}
    bool is_default() const { return begin == default_begin && end == default_end; }
    static const string_type default_begin;
    static const string_type default_end;
};

template <typename string_type>
const string_type delimiter_set<string_type>::default_begin(2, '{');
template <typename string_type>
const string_type delimiter_set<string_type>::default_end(2, '}');

template <typename string_type>
class basic_context {
public:
    virtual void push(const basic_data<string_type>* data) = 0;
    virtual void pop() = 0;

    virtual const basic_data<string_type>* get(const string_type& name) const = 0;
    virtual const basic_data<string_type>* get_partial(const string_type& name) const = 0;
};

template <typename string_type>
class context : public basic_context<string_type> {
public:
    context(const basic_data<string_type>* data) {
        push(data);
    }

    context() {
    }

    virtual void push(const basic_data<string_type>* data) override {
        items_.insert(items_.begin(), data);
    }

    virtual void pop() override {
        items_.erase(items_.begin());
    }
    
    virtual const basic_data<string_type>* get(const string_type& name) const override {
        // process {{.}} name
        if (name.size() == 1 && name.at(0) == '.') {
            return items_.front();
        }
        if (name.find('.') == string_type::npos) {
            // process normal name without having to split which is slower
            for (const auto& item : items_) {
                const auto var = item->get(name);
                if (var) {
                    return var;
                }
            }
            return nullptr;
        }
        // process x.y-like name
        const auto names = split(name, '.');
        for (const auto& item : items_) {
            auto var = item;
            for (const auto& n : names) {
                var = var->get(n);
                if (!var) {
                    break;
                }
            }
            if (var) {
                return var;
            }
        }
        return nullptr;
    }

    virtual const basic_data<string_type>* get_partial(const string_type& name) const override {
        for (const auto& item : items_) {
            const auto var = item->get(name);
            if (var) {
                return var;
            }
        }
        return nullptr;
    }

    context(const context&) = delete;
    context& operator= (const context&) = delete;

private:
    std::vector<const basic_data<string_type>*> items_;
};

template <typename StringType>
class basic_mustache {
public:
    using string_type = StringType;

    basic_mustache(const string_type& input)
        : basic_mustache() {
        context<string_type> ctx;
        context_internal context{ctx};
        parse(input, context);
    }

    bool is_valid() const {
        return errorMessage_.empty();
    }
    
    const string_type& error_message() const {
        return errorMessage_;
    }

    using escape_handler = std::function<string_type(const string_type&)>;
    void set_custom_escape(const escape_handler& escape_fn) {
        escape_ = escape_fn;
    }

    template <typename stream_type>
    stream_type& render(const basic_data<string_type>& data, stream_type& stream) {
        render(data, [&stream](const string_type& str) {
            stream << str;
        });
        return stream;
    }
    
    string_type render(const basic_data<string_type>& data) {
        std::basic_ostringstream<typename string_type::value_type> ss;
        return render(data, ss).str();
    }

    string_type render(basic_context<string_type>& ctx) {
        std::basic_ostringstream<typename string_type::value_type> ss;
        context_internal context{ctx};
        render([&ss](const string_type& str) {
            ss << str;
        }, context);
        return ss.str();
    }

    using RenderHandler = std::function<void(const string_type&)>;
    void render(const basic_data<string_type>& data, const RenderHandler& handler) {
        if (!is_valid()) {
            return;
        }
        context<string_type> ctx{&data};
        context_internal context{ctx};
        render(handler, context);
    }

private:
    using StringSizeType = typename string_type::size_type;
    
    class Tag {
    public:
        enum class Type {
            Invalid,
            Variable,
            UnescapedVariable,
            SectionBegin,
            SectionEnd,
            SectionBeginInverted,
            Comment,
            Partial,
            SetDelimiter,
        };
        string_type name;
        Type type = Type::Invalid;
        std::shared_ptr<string_type> sectionText;
        std::shared_ptr<delimiter_set<string_type>> delimiterSet;
        bool isSectionBegin() const {
            return type == Type::SectionBegin || type == Type::SectionBeginInverted;
        }
        bool isSectionEnd() const {
            return type == Type::SectionEnd;
        }
    };
    
    class component {
    public:
        string_type text;
        Tag tag;
        std::vector<component> children;
        StringSizeType position = string_type::npos;
        bool isText() const {
            return tag.type == Tag::Type::Invalid;
        }
        component() {}
        component(const string_type& t, StringSizeType p) : text(t), position(p) {}
    };

    class context_internal {
    public:
        basic_context<string_type>& ctx;
        delimiter_set<string_type> delimiterSet;

        context_internal(basic_context<string_type>& a_ctx)
            : ctx(a_ctx)
        {
        }
    };

    class context_pusher {
    public:
        context_pusher(context_internal& ctx, const basic_data<string_type>* data) : ctx_(ctx) {
            ctx.ctx.push(data);
        }
        ~context_pusher() {
            ctx_.ctx.pop();
        }
        context_pusher(const context_pusher&) = delete;
        context_pusher& operator= (const context_pusher&) = delete;
    private:
        context_internal& ctx_;
    };

    basic_mustache()
        : escape_(html_escape<string_type>)
    {
    }
    
    basic_mustache(const string_type& input, context_internal& ctx)
        : basic_mustache() {
        parse(input, ctx);
    }

    void parse(const string_type& input, context_internal& ctx) {
        using streamstring = std::basic_ostringstream<typename string_type::value_type>;
        
        const string_type braceDelimiterEndUnescaped(3, '}');
        const StringSizeType inputSize{input.size()};
        
        bool currentDelimiterIsBrace{ctx.delimiterSet.is_default()};
        
        std::vector<component*> sections{&rootComponent_};
        std::vector<StringSizeType> sectionStarts;
        
        StringSizeType inputPosition{0};
        while (inputPosition != inputSize) {
            
            // Find the next tag start delimiter
            const StringSizeType tagLocationStart{input.find(ctx.delimiterSet.begin, inputPosition)};
            if (tagLocationStart == string_type::npos) {
                // No tag found. Add the remaining text.
                const component comp{{input, inputPosition, inputSize - inputPosition}, inputPosition};
                sections.back()->children.push_back(comp);
                break;
            } else if (tagLocationStart != inputPosition) {
                // Tag found, add text up to this tag.
                const component comp{{input, inputPosition, tagLocationStart - inputPosition}, inputPosition};
                sections.back()->children.push_back(comp);
            }
            
            // Find the next tag end delimiter
            StringSizeType tagContentsLocation{tagLocationStart + ctx.delimiterSet.begin.size()};
            const bool tagIsUnescapedVar{currentDelimiterIsBrace && tagLocationStart != (inputSize - 2) && input.at(tagContentsLocation) == ctx.delimiterSet.begin.at(0)};
            const string_type& currentTagDelimiterEnd{tagIsUnescapedVar ? braceDelimiterEndUnescaped : ctx.delimiterSet.end};
            const auto currentTagDelimiterEndSize = currentTagDelimiterEnd.size();
            if (tagIsUnescapedVar) {
                ++tagContentsLocation;
            }
            StringSizeType tagLocationEnd{input.find(currentTagDelimiterEnd, tagContentsLocation)};
            if (tagLocationEnd == string_type::npos) {
                streamstring ss;
                ss << "Unclosed tag at " << tagLocationStart;
                errorMessage_.assign(ss.str());
                return;
            }
            
            // Parse tag
            const string_type tagContents{trim(string_type{input, tagContentsLocation, tagLocationEnd - tagContentsLocation})};
            component comp;
            if (!tagContents.empty() && tagContents[0] == '=') {
                if (!parseSetDelimiterTag(tagContents, ctx.delimiterSet)) {
                    streamstring ss;
                    ss << "Invalid set delimiter tag at " << tagLocationStart;
                    errorMessage_.assign(ss.str());
                    return;
                }
                currentDelimiterIsBrace = ctx.delimiterSet.is_default();
                comp.tag.type = Tag::Type::SetDelimiter;
                comp.tag.delimiterSet.reset(new delimiter_set<string_type>(ctx.delimiterSet));
            }
            if (comp.tag.type != Tag::Type::SetDelimiter) {
                parseTagContents(tagIsUnescapedVar, tagContents, comp.tag);
            }
            comp.position = tagLocationStart;
            sections.back()->children.push_back(comp);
            
            // Start next search after this tag
            inputPosition = tagLocationEnd + currentTagDelimiterEndSize;

            // Push or pop sections
            if (comp.tag.isSectionBegin()) {
                sections.push_back(&sections.back()->children.back());
                sectionStarts.push_back(inputPosition);
            } else if (comp.tag.isSectionEnd()) {
                if (sections.size() == 1) {
                    streamstring ss;
                    ss << "Unopened section \"" << comp.tag.name << "\" at " << comp.position;
                    errorMessage_.assign(ss.str());
                    return;
                }
                sections.back()->tag.sectionText.reset(new string_type(input.substr(sectionStarts.back(), tagLocationStart - sectionStarts.back())));
                sections.pop_back();
                sectionStarts.pop_back();
            }
        }
        
        // Check for sections without an ending tag
        walk([this](component& comp) -> WalkControl {
            if (!comp.tag.isSectionBegin()) {
                return WalkControl::Continue;
            }
            if (comp.children.empty() || !comp.children.back().tag.isSectionEnd() || comp.children.back().tag.name != comp.tag.name) {
                streamstring ss;
                ss << "Unclosed section \"" << comp.tag.name << "\" at " << comp.position;
                errorMessage_.assign(ss.str());
                return WalkControl::Stop;
            }
            comp.children.pop_back(); // remove now useless end section component
            return WalkControl::Continue;
        });
        if (!errorMessage_.empty()) {
            return;
        }
    }
    
    enum class WalkControl {
        Continue,
        Stop,
        Skip,
    };
    using WalkCallback = std::function<WalkControl(component&)>;
    
    void walk(const WalkCallback& callback) {
        walkChildren(callback, rootComponent_);
    }

    void walkChildren(const WalkCallback& callback, component& comp) {
        for (auto& childComp : comp.children) {
            if (walkComponent(callback, childComp) != WalkControl::Continue) {
                break;
            }
        }
    }
    
    WalkControl walkComponent(const WalkCallback& callback, component& comp) {
        WalkControl control{callback(comp)};
        if (control == WalkControl::Stop) {
            return control;
        } else if (control == WalkControl::Skip) {
            return WalkControl::Continue;
        }
        for (auto& childComp : comp.children) {
            control = walkComponent(callback, childComp);
            assert(control == WalkControl::Continue);
        }
        return control;
    }
    
    bool isSetDelimiterValid(const string_type& delimiter) {
        // "Custom delimiters may not contain whitespace or the equals sign."
        for (const auto ch : delimiter) {
            if (ch == '=' || isspace(ch)) {
                return false;
            }
        }
        return true;
    }
    
    bool parseSetDelimiterTag(const string_type& contents, delimiter_set<string_type>& delimiterSet) {
        // Smallest legal tag is "=X X="
        if (contents.size() < 5) {
            return false;
        }
        if (contents.back() != '=') {
            return false;
        }
        const auto contentsSubstr = trim(contents.substr(1, contents.size() - 2));
        const auto spacepos = contentsSubstr.find(' ');
        if (spacepos == string_type::npos) {
            return false;
        }
        const auto nonspace = contentsSubstr.find_first_not_of(' ', spacepos + 1);
        assert(nonspace != string_type::npos);
        const string_type begin = contentsSubstr.substr(0, spacepos);
        const string_type end = contentsSubstr.substr(nonspace, contentsSubstr.size() - nonspace);
        if (!isSetDelimiterValid(begin) || !isSetDelimiterValid(end)) {
            return false;
        }
        delimiterSet.begin = begin;
        delimiterSet.end = end;
        return true;
    }
    
    void parseTagContents(bool isUnescapedVar, const string_type& contents, Tag& tag) {
        if (isUnescapedVar) {
            tag.type = Tag::Type::UnescapedVariable;
            tag.name = contents;
        } else if (contents.empty()) {
            tag.type = Tag::Type::Variable;
            tag.name.clear();
        } else {
            switch (contents.at(0)) {
                case '#':
                    tag.type = Tag::Type::SectionBegin;
                    break;
                case '^':
                    tag.type = Tag::Type::SectionBeginInverted;
                    break;
                case '/':
                    tag.type = Tag::Type::SectionEnd;
                    break;
                case '>':
                    tag.type = Tag::Type::Partial;
                    break;
                case '&':
                    tag.type = Tag::Type::UnescapedVariable;
                    break;
                case '!':
                    tag.type = Tag::Type::Comment;
                    break;
                default:
                    tag.type = Tag::Type::Variable;
                    break;
            }
            if (tag.type == Tag::Type::Variable) {
                tag.name = contents;
            } else {
                string_type name{contents};
                name.erase(name.begin());
                tag.name = trim(name);
            }
        }
    }

    string_type render(context_internal& ctx) {
        std::basic_ostringstream<typename string_type::value_type> ss;
        render([&ss](const string_type& str) {
            ss << str;
        }, ctx);
        return ss.str();
    }

    void render(const RenderHandler& handler, context_internal& ctx) {
        walk([&handler, &ctx, this](component& comp) -> WalkControl {
            return renderComponent(handler, ctx, comp);
        });
    }

    WalkControl renderComponent(const RenderHandler& handler, context_internal& ctx, component& comp) {
        if (comp.isText()) {
            handler(comp.text);
            return WalkControl::Continue;
        }
        
        const Tag& tag{comp.tag};
        const basic_data<string_type>* var = nullptr;
        switch (tag.type) {
            case Tag::Type::Variable:
            case Tag::Type::UnescapedVariable:
                if ((var = ctx.ctx.get(tag.name)) != nullptr) {
                    if (!renderVariable(handler, var, ctx, tag.type == Tag::Type::Variable)) {
                        return WalkControl::Stop;
                    }
                }
                break;
            case Tag::Type::SectionBegin:
                if ((var = ctx.ctx.get(tag.name)) != nullptr) {
                    if (var->is_lambda() || var->is_lambda2()) {
                        if (!renderLambda(handler, var, ctx, RenderLambdaEscape::Optional, *comp.tag.sectionText, true)) {
                            return WalkControl::Stop;
                        }
                    } else if (!var->is_false() && !var->is_empty_list()) {
                        renderSection(handler, ctx, comp, var);
                    }
                }
                return WalkControl::Skip;
            case Tag::Type::SectionBeginInverted:
                if ((var = ctx.ctx.get(tag.name)) == nullptr || var->is_false() || var->is_empty_list()) {
                    renderSection(handler, ctx, comp, var);
                }
                return WalkControl::Skip;
            case Tag::Type::Partial:
                if ((var = ctx.ctx.get_partial(tag.name)) != nullptr && (var->is_partial() || var->is_string())) {
                    const auto partial_result = var->is_partial() ? var->partial_value()() : var->string_value();
                    basic_mustache tmpl{partial_result};
                    tmpl.set_custom_escape(escape_);
                    if (!tmpl.is_valid()) {
                        errorMessage_ = tmpl.error_message();
                    } else {
                        tmpl.render(handler, ctx);
                        if (!tmpl.is_valid()) {
                            errorMessage_ = tmpl.error_message();
                        }
                    }
                    if (!tmpl.is_valid()) {
                        return WalkControl::Stop;
                    }
                }
                break;
            case Tag::Type::SetDelimiter:
                ctx.delimiterSet = *comp.tag.delimiterSet;
                break;
            default:
                break;
        }
        
        return WalkControl::Continue;
    }

    enum class RenderLambdaEscape {
        Escape,
        Unescape,
        Optional,
    };
    
    bool renderLambda(const RenderHandler& handler, const basic_data<string_type>* var, context_internal& ctx, RenderLambdaEscape escape, const string_type& text, bool parseWithSameContext) {
        const typename basic_renderer<string_type>::type2 render2 = [this, &handler, var, &ctx, parseWithSameContext, escape](const string_type& text, bool escaped) {
            const auto processTemplate = [this, &handler, var, &ctx, escape, escaped](basic_mustache& tmpl) -> string_type {
                if (!tmpl.is_valid()) {
                    errorMessage_ = tmpl.error_message();
                    return {};
                }
                const string_type str{tmpl.render(ctx)};
                if (!tmpl.is_valid()) {
                    errorMessage_ = tmpl.error_message();
                    return {};
                }
                bool doEscape = false;
                switch (escape) {
                    case RenderLambdaEscape::Escape:
                        doEscape = true;
                        break;
                    case RenderLambdaEscape::Unescape:
                        doEscape = false;
                        break;
                    case RenderLambdaEscape::Optional:
                        doEscape = escaped;
                        break;
                }
                return doEscape ? escape_(str) : str;
            };
            if (parseWithSameContext) {
                basic_mustache tmpl{text, ctx};
                tmpl.set_custom_escape(escape_);
                return processTemplate(tmpl);
            }
            basic_mustache tmpl{text};
            tmpl.set_custom_escape(escape_);
            return processTemplate(tmpl);
        };
        const typename basic_renderer<string_type>::type1 render = [&render2](const string_type& text) {
            return render2(text, false);
        };
        if (var->is_lambda2()) {
            const basic_renderer<string_type> renderer{render, render2};
            handler(var->lambda2_value()(text, renderer));
        } else {
            handler(render(var->lambda_value()(text)));
        }
        return errorMessage_.empty();
    }
    
    bool renderVariable(const RenderHandler& handler, const basic_data<string_type>* var, context_internal& ctx, bool escaped) {
        if (var->is_string()) {
            const auto varstr = var->string_value();
            handler(escaped ? escape_(varstr) : varstr);
        } else if (var->is_lambda()) {
            const RenderLambdaEscape escapeOpt = escaped ? RenderLambdaEscape::Escape : RenderLambdaEscape::Unescape;
            return renderLambda(handler, var, ctx, escapeOpt, {}, false);
        } else if (var->is_lambda2()) {
            using streamstring = std::basic_ostringstream<typename string_type::value_type>;
            streamstring ss;
            ss << "Lambda with render argument is not allowed for regular variables";
            errorMessage_ = ss.str();
            return false;
        }
        return true;
    }

    void renderSection(const RenderHandler& handler, context_internal& ctx, component& incomp, const basic_data<string_type>* var) {
        const auto callback = [&handler, &ctx, this](component& comp) -> WalkControl {
            return renderComponent(handler, ctx, comp);
        };
        if (var && var->is_non_empty_list()) {
            for (const auto& item : var->list_value()) {
                const context_pusher ctxpusher{ctx, &item};
                walkChildren(callback, incomp);
            }
        } else if (var) {
            const context_pusher ctxpusher{ctx, var};
            walkChildren(callback, incomp);
        } else {
            walkChildren(callback, incomp);
        }
    }

private:
    string_type errorMessage_;
    component rootComponent_;
    escape_handler escape_;
};

using mustache = basic_mustache<std::string>;
using data = basic_data<mustache::string_type>;
using object = basic_object<mustache::string_type>;
using list = basic_list<mustache::string_type>;
using partial = basic_partial<mustache::string_type>;
using renderer = basic_renderer<mustache::string_type>;
using lambda = basic_lambda<mustache::string_type>;
using lambda2 = basic_lambda2<mustache::string_type>;
using lambda_t = basic_lambda_t<mustache::string_type>;

using mustachew = basic_mustache<std::wstring>;
using dataw = basic_data<mustachew::string_type>;

} // namespace mustache
} // namespace kainjow

#endif // KAINJOW_MUSTACHE_HPP