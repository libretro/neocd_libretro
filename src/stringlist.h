#ifndef STRINGLIST_H
#define STRINGLIST_H 1

#include <lists/string_list.h>

class StringList
{
public:
    explicit StringList(string_list* list = nullptr) : m_list(list)
    { }

    ~StringList()
    {
        release();
    }

    bool isNull() const
    {
        return (m_list == nullptr);
    }

    bool isEmpty() const
    {
        if (!m_list)
            return true;

        return (m_list->size == 0);
    }

    size_t count() const
    {
        if (!m_list)
            return 0;

        return static_cast<size_t>(m_list->size);
    }

    const string_list_elem* begin() const
    {
        if (!m_list)
            return nullptr;

        return &m_list->elems[0];
    }
    
    const string_list_elem* end() const
    {
        if (!m_list)
            return nullptr;

        return &m_list->elems[m_list->size];
    }

    StringList& operator=(string_list* list)
    {
        release();

        m_list = list;

        return *this;
    }

protected:
    void release()
    {
        string_list_free(m_list);
        m_list = nullptr;
    }

    string_list* m_list;
};

#endif