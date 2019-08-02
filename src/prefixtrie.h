#ifndef BITCOIN_PREFIXTRIE_H
#define BITCOIN_PREFIXTRIE_H

#include <algorithm>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

template <typename TKey, typename TData>
class CPrefixTrie
{
    class Node
    {
        template <bool>
        friend class Iterator;
        friend class CPrefixTrie<TKey, TData>;
        std::map<TKey, std::shared_ptr<Node>> children;

    public:
        Node() = default;
        Node(const Node&) = delete;
        Node(Node&& o) noexcept = default;
        Node& operator=(Node&& o) noexcept = default;
        Node& operator=(const Node&) = delete;
        std::shared_ptr<TData> data;
    };

    using TChildren = decltype(Node::children);

    template <bool IsConst>
    class Iterator
    {
        template <bool>
        friend class Iterator;
        friend class CPrefixTrie<TKey, TData>;

        using TKeyRef = std::reference_wrapper<const TKey>;
        using TDataRef = std::reference_wrapper<TData>;
        using TPair = std::pair<TKeyRef, TDataRef>;

        TKey name;
        std::weak_ptr<Node> node;

        struct Bookmark {
            TKey name;
            std::weak_ptr<Node> parent;
            typename TChildren::iterator it;
            typename TChildren::iterator end;
        };

        std::vector<Bookmark> stack;

    public:
        // Iterator traits
        using value_type = TPair;
        using pointer = typename std::conditional<IsConst, const TData* const, TData* const>::type;
        using reference = typename std::conditional<IsConst, const TPair, TPair>::type;
        using difference_type = std::ptrdiff_t;
        using iterator_category = std::forward_iterator_tag;

        Iterator() = delete;
        Iterator(const Iterator&) = default;
        Iterator(Iterator&& o) noexcept = default;
        Iterator(const TKey& name, const std::shared_ptr<Node>& node) noexcept;
        template <bool C>
        inline Iterator(const Iterator<C>& o) noexcept
        {
            *this = o;
        }

        Iterator& operator=(const Iterator&) = default;
        Iterator& operator=(Iterator&& o) = default;
        template <bool C>
        Iterator& operator=(const Iterator<C>& o) noexcept;

        bool hasNext() const;
        Iterator& operator++();
        Iterator operator++(int);

        operator bool() const;

        bool operator==(const Iterator& o) const;
        bool operator!=(const Iterator& o) const;

        reference operator*() const;
        pointer operator->() const;

        const TKey& key() const;

        TData& data();
        const TData& data() const;

        std::size_t depth() const;

        bool hasChildren() const;
        std::vector<Iterator> children() const;
    };

    size_t size;
    std::shared_ptr<Node> root;

    template <typename TNode>
    using callback = std::function<void(const TKey&, TNode)>;

    template <typename TIterator, typename TNode>
    static TIterator find(const TKey& key, TNode node, TIterator end);

    template <typename TNode>
    static bool find(const TKey& key, TNode node, const callback<TNode>& cb);

    std::shared_ptr<Node>& insert(const TKey& key, std::shared_ptr<Node>& node);
    void erase(const TKey& key, std::shared_ptr<Node>& node);

public:
    using iterator = Iterator<false>;
    using const_iterator = Iterator<true>;

    CPrefixTrie();

    template <typename TDataUni>
    iterator insert(const TKey& key, TDataUni&& data);
    template <typename TDataUni>
    iterator insert(iterator& it, const TKey& key, TDataUni&& data);

    iterator copy(const_iterator it);

    iterator find(const TKey& key);
    const_iterator find(const TKey& key) const;
    iterator find(iterator& it, const TKey& key);
    const_iterator find(const_iterator& it, const TKey& key) const;

    bool contains(const TKey& key) const;

    TData& at(const TKey& key);

    std::vector<iterator> nodes(const TKey& key) const;

    bool erase(const TKey& key);

    void clear();
    bool empty() const;

    size_t height() const;

    iterator begin();
    iterator end();

    const_iterator cbegin();
    const_iterator cend();

    const_iterator begin() const;
    const_iterator end() const;
};

template <typename T, typename O>
inline bool operator==(const std::reference_wrapper<T>& ref, const O& obj)
{
    return ref.get() == obj;
}

template <typename T, typename O>
inline bool operator!=(const std::reference_wrapper<T>& ref, const O& obj)
{
    return !(ref == obj);
}

template <typename T>
inline bool operator==(const std::reference_wrapper<std::shared_ptr<T>>& ref, const T& obj)
{
    auto ptr = ref.get();
    return ptr && *ptr == obj;
}

template <typename T>
inline bool operator!=(const std::reference_wrapper<std::shared_ptr<T>>& ref, const T& obj)
{
    return !(ref == obj);
}

template <typename T>
inline bool operator==(const std::reference_wrapper<std::unique_ptr<T>>& ref, const T& obj)
{
    auto ptr = ref.get();
    return ptr && *ptr == obj;
}

template <typename T>
inline bool operator!=(const std::reference_wrapper<std::unique_ptr<T>>& ref, const T& obj)
{
    return !(ref == obj);
}

template <typename TKey>
static std::size_t match(const TKey& a, const TKey& b)
{
    std::size_t count = 0;
    auto ait = a.cbegin(), aend = a.cend();
    auto bit = b.cbegin(), bend = b.cend();
    while (ait != aend && bit != bend) {
        if (*ait != *bit) break;
        ++count;
        ++ait;
        ++bit;
    }
    return count;
}

#endif // BITCOIN_PREFIXTRIE_H
