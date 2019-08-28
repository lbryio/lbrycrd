
#include "prefixtrie.h"
#include "claimtrie.h"

template <typename TKey, typename TData>
template <bool IsConst>
CPrefixTrie<TKey, TData>::Iterator<IsConst>::Iterator(const TKey& name, const std::shared_ptr<Node>& node) noexcept : name(name), node(node)
{
}

template <typename TKey, typename TData>
template <bool IsConst>
template <bool C>
typename CPrefixTrie<TKey, TData>::template Iterator<IsConst>& CPrefixTrie<TKey, TData>::Iterator<IsConst>::operator=(const CPrefixTrie<TKey, TData>::Iterator<C>& o) noexcept
{
    name = o.name;
    node = o.node;
    stack.clear();
    stack.reserve(o.stack.size());
    for (auto& i : o.stack)
        stack.push_back(Bookmark{i.name, i.parent, i.it, i.end});
    return *this;
}

template <typename TKey, typename TData>
template <bool IsConst>
bool CPrefixTrie<TKey, TData>::Iterator<IsConst>::hasNext() const
{
    auto shared = node.lock();
    if (!shared) return false;
    if (!shared->children.empty()) return true;
    for (auto it = stack.rbegin(); it != stack.rend(); ++it) {
        auto mark = *it; // copy
        if (++mark.it != mark.end)
            return true;
    }
    return false;
}

template <typename TKey, typename TData>
template <bool IsConst>
typename CPrefixTrie<TKey, TData>::template Iterator<IsConst>& CPrefixTrie<TKey, TData>::Iterator<IsConst>::operator++()
{
    auto shared = node.lock();
    assert(shared);
    // going in pre-order (NLR). See https://en.wikipedia.org/wiki/Tree_traversal
    // if there are any children we have to go there first
    if (!shared->children.empty()) {
        auto& children = shared->children;
        auto it = children.begin();
        stack.emplace_back(Bookmark{name, shared, it, children.end()});
        auto& postfix = it->first;
        name.insert(name.end(), postfix.begin(), postfix.end());
        node = it->second;
        return *this;
    }

    // move to next sibling:
    while (!stack.empty()) {
        auto& back = stack.back();
        if (++back.it != back.end) {
            name = back.name;
            auto& postfix = back.it->first;
            name.insert(name.end(), postfix.begin(), postfix.end());
            node = back.it->second;
            return *this;
        }
        stack.pop_back();
    }

    // must be at the end:
    node.reset();
    name = TKey();
    return *this;
}

template <typename TKey, typename TData>
template <bool IsConst>
typename CPrefixTrie<TKey, TData>::template Iterator<IsConst> CPrefixTrie<TKey, TData>::Iterator<IsConst>::operator++(int x)
{
    auto ret = *this;
    ++(*this);
    return ret;
}

template <typename TKey, typename TData>
template <bool IsConst>
CPrefixTrie<TKey, TData>::Iterator<IsConst>::operator bool() const
{
    return !node.expired();
}

template <typename TKey, typename TData>
template <bool IsConst>
bool CPrefixTrie<TKey, TData>::Iterator<IsConst>::operator==(const Iterator& o) const
{
    return node.lock() == o.node.lock();
}

template <typename TKey, typename TData>
template <bool IsConst>
bool CPrefixTrie<TKey, TData>::Iterator<IsConst>::operator!=(const Iterator& o) const
{
    return !(*this == o);
}

template <typename TKey, typename TData>
template <bool IsConst>
typename CPrefixTrie<TKey, TData>::template Iterator<IsConst>::reference CPrefixTrie<TKey, TData>::Iterator<IsConst>::operator*() const
{
    assert(!node.expired());
    return TPair(name, node.lock()->data);
}

template <typename TKey, typename TData>
template <bool IsConst>
typename CPrefixTrie<TKey, TData>::template Iterator<IsConst>::pointer CPrefixTrie<TKey, TData>::Iterator<IsConst>::operator->() const
{
    assert(!node.expired());
    return &(node.lock()->data);
}

template <typename TKey, typename TData>
template <bool IsConst>
const TKey& CPrefixTrie<TKey, TData>::Iterator<IsConst>::key() const
{
    return name;
}

template <typename TKey, typename TData>
template <bool IsConst>
TData& CPrefixTrie<TKey, TData>::Iterator<IsConst>::data()
{
    assert(!node.expired());
    return node.lock()->data;
}

template <typename TKey, typename TData>
template <bool IsConst>
const TData& CPrefixTrie<TKey, TData>::Iterator<IsConst>::data() const
{
    assert(!node.expired());
    return node.lock()->data;
}

template <typename TKey, typename TData>
template <bool IsConst>
std::size_t CPrefixTrie<TKey, TData>::Iterator<IsConst>::depth() const
{
    return stack.size();
}

template <typename TKey, typename TData>
template <bool IsConst>
bool CPrefixTrie<TKey, TData>::Iterator<IsConst>::hasChildren() const
{
    auto shared = node.lock();
    return shared && !shared->children.empty();
}

template <typename TKey, typename TData>
template <bool IsConst>
std::vector<typename CPrefixTrie<TKey, TData>::template Iterator<IsConst>> CPrefixTrie<TKey, TData>::Iterator<IsConst>::children() const
{
    auto shared = node.lock();
    if (!shared) return {};
    std::vector<Iterator<IsConst>> ret;
    ret.reserve(shared->children.size());
    for (auto& child : shared->children) {
        auto key = name;
        key.insert(key.end(), child.first.begin(), child.first.end());
        ret.emplace_back(key, child.second);
    }
    return ret;
}

template <typename TKey, typename TData>
template <typename TIterator, typename TNode>
TIterator CPrefixTrie<TKey, TData>::find(const TKey& key, TNode node, TIterator end)
{
    TIterator it(key, TNode());
    using CBType = callback<TNode>;
    CBType cb = [&it](const TKey&, TNode node) {
        it.node = node;
    };
    return find(key, node, cb) ? it : end;
}

template <typename TKey, typename TData>
template <typename TNode>
bool CPrefixTrie<TKey, TData>::find(const TKey& key, TNode node, const callback<TNode>& cb)
{
    auto& children = node->children;
    if (children.empty()) return false;
    auto it = children.lower_bound(key);
    if (it != children.end() && it->first == key) {
        cb(key, it->second);
        return true;
    }
    if (it != children.begin()) --it;
    const auto count = match(key, it->first);
    if (count != it->first.size()) return false;
    if (count == key.size()) return false;
    cb(it->first, it->second);
    return find(TKey(key.begin() + count, key.end()), it->second, cb);
}

template <typename TKey, typename TData>
std::shared_ptr<typename CPrefixTrie<TKey, TData>::Node>& CPrefixTrie<TKey, TData>::insert(const TKey& key, std::shared_ptr<typename CPrefixTrie<TKey, TData>::Node>& node)
{
    std::size_t count = 0;
    auto& children = node->children;
    auto it = children.lower_bound(key);
    if (it != children.end()) {
        if (it->first == key)
            return it->second;
        count = match(key, it->first);
    }
    if (count == 0 && it != children.begin()) {
        --it;
        count = match(key, it->first);
    }
    if (count == 0) {
        ++size;
        it = children.emplace(key, std::make_shared<Node>()).first;
        return it->second;
    }
    if (count < it->first.size()) {
        const TKey prefix(key.begin(), key.begin() + count);
        const TKey postfix(it->first.begin() + count, it->first.end());
        auto nodes = std::move(it->second);
        children.erase(it);
        ++size;
        it = children.emplace(prefix, std::make_shared<Node>()).first;
        it->second->children.emplace(postfix, std::move(nodes));
        if (key.size() == count)
            return it->second;
    }
    return insert(TKey(key.begin() + count, key.end()), it->second);
}

template <typename TKey, typename TData>
void CPrefixTrie<TKey, TData>::erase(const TKey& key, std::shared_ptr<Node>& node)
{
    std::vector<typename TChildren::value_type> nodes;
    nodes.emplace_back(TKey(), node);
    using CBType = callback<std::shared_ptr<Node>>;
    CBType cb = [&nodes](const TKey& k, std::shared_ptr<Node> n) {
        nodes.emplace_back(k, n);
    };
    if (!find(key, node, cb))
        return;

    nodes.back().second->data = {};
    for (; nodes.size() > 1; nodes.pop_back()) {
        // if we have only one child and no data ourselves, bring them up to our level
        auto& cNode = nodes.back().second;
        auto onlyOneChild = cNode->children.size() == 1;
        auto noData = cNode->data.empty();
        if (onlyOneChild && noData) {
            auto child = cNode->children.begin();
            auto& prefix = nodes.back().first;
            auto newKey = prefix;
            auto& postfix = child->first;
            newKey.insert(newKey.end(), postfix.begin(), postfix.end());
            auto& pNode = nodes[nodes.size() - 2].second;
            pNode->children.emplace(newKey, std::move(child->second));
            pNode->children.erase(prefix);
            --size;
            continue;
        }

        auto noChildren = cNode->children.empty();
        if (noChildren && noData) {
            auto& pNode = nodes[nodes.size() - 2].second;
            pNode->children.erase(nodes.back().first);
            --size;
            continue;
        }
        break;
    }
}

template <typename TKey, typename TData>
CPrefixTrie<TKey, TData>::CPrefixTrie() : size(0), root(std::make_shared<Node>())
{
}

template <typename TKey, typename TData>
template <typename TDataUni>
typename CPrefixTrie<TKey, TData>::iterator CPrefixTrie<TKey, TData>::insert(const TKey& key, TDataUni&& data)
{
    auto& node = key.empty() ? root : insert(key, root);
    node->data = std::forward<TDataUni>(data);
    return key.empty() ? begin() : iterator{key, node};
}

template <typename TKey, typename TData>
typename CPrefixTrie<TKey, TData>::iterator CPrefixTrie<TKey, TData>::copy(CPrefixTrie<TKey, TData>::const_iterator it)
{
    auto& key = it.key();
    auto& node = key.empty() ? root : insert(key, root);
    node->data = it.node.lock()->data;
    return key.empty() ? begin() : iterator{key, node};
}

template <typename TKey, typename TData>
template <typename TDataUni>
typename CPrefixTrie<TKey, TData>::iterator CPrefixTrie<TKey, TData>::insert(CPrefixTrie<TKey, TData>::iterator& it, const TKey& key, TDataUni&& data)
{
    auto shared = it.node.lock();
    assert(shared);
    auto copy = it;
    if (!key.empty()) {
        auto name = it.key();
        name.insert(name.end(), key.begin(), key.end());
        auto& node = insert(key, shared);
        copy = iterator{name, node};
    }
    copy.node.lock()->data = std::forward<TDataUni>(data);
    return copy;
}

template <typename TKey, typename TData>
typename CPrefixTrie<TKey, TData>::iterator CPrefixTrie<TKey, TData>::find(const TKey& key)
{
    if (empty()) return end();
    if (key.empty()) return {key, root};
    return find(key, root, end());
}

template <typename TKey, typename TData>
typename CPrefixTrie<TKey, TData>::const_iterator CPrefixTrie<TKey, TData>::find(const TKey& key) const
{
    if (empty()) return end();
    if (key.empty()) return {key, root};
    return find(key, root, end());
}

template <typename TKey, typename TData>
typename CPrefixTrie<TKey, TData>::iterator CPrefixTrie<TKey, TData>::find(CPrefixTrie<TKey, TData>::iterator& it, const TKey& key)
{
    if (key.empty()) return it;
    auto shared = it.node.lock();
    assert(shared);
    return find(key, shared, end());
}

template <typename TKey, typename TData>
typename CPrefixTrie<TKey, TData>::const_iterator CPrefixTrie<TKey, TData>::find(CPrefixTrie<TKey, TData>::const_iterator& it, const TKey& key) const
{
    if (key.empty()) return it;
    auto shared = it.node.lock();
    assert(shared);
    return find(key, shared, end());
}

template <typename TKey, typename TData>
bool CPrefixTrie<TKey, TData>::contains(const TKey& key) const
{
    return find(key) != end();
}

template <typename TKey, typename TData>
TData& CPrefixTrie<TKey, TData>::at(const TKey& key)
{
    return find(key).data();
}

template <typename TKey, typename TData>
std::vector<typename CPrefixTrie<TKey, TData>::iterator> CPrefixTrie<TKey, TData>::nodes(const TKey& key) const
{
    std::vector<iterator> ret;
    if (empty()) return ret;
    ret.reserve(1 + key.size());
    ret.emplace_back(TKey{}, root);
    if (key.empty()) return ret;
    TKey name;
    using CBType = callback<std::shared_ptr<Node>>;
    CBType cb = [&name, &ret](const TKey& key, std::shared_ptr<Node> node) {
        name.insert(name.end(), key.begin(), key.end());
        ret.emplace_back(name, node);
    };
    find(key, root, cb);
    return ret;
}

template <typename TKey, typename TData>
bool CPrefixTrie<TKey, TData>::erase(const TKey& key)
{
    auto size_was = height();
    if (key.empty()) {
        root->data = {};
    } else {
        erase(key, root);
    }
    return size_was != height();
}

template <typename TKey, typename TData>
void CPrefixTrie<TKey, TData>::clear()
{
    size = 0;
    root->data = {};
    root->children.clear();
}

template <typename TKey, typename TData>
bool CPrefixTrie<TKey, TData>::empty() const
{
    return height() == 0;
}

template <typename TKey, typename TData>
std::size_t CPrefixTrie<TKey, TData>::height() const
{
    return size + (root->data.empty() ? 0 : 1);
}

template <typename TKey, typename TData>
typename CPrefixTrie<TKey, TData>::iterator CPrefixTrie<TKey, TData>::begin()
{
    return find(TKey());
}

template <typename TKey, typename TData>
typename CPrefixTrie<TKey, TData>::iterator CPrefixTrie<TKey, TData>::end()
{
    return iterator{TKey(), std::shared_ptr<Node>{}};
}

template <typename TKey, typename TData>
typename CPrefixTrie<TKey, TData>::const_iterator CPrefixTrie<TKey, TData>::cbegin()
{
    return find(TKey());
}

template <typename TKey, typename TData>
typename CPrefixTrie<TKey, TData>::const_iterator CPrefixTrie<TKey, TData>::cend()
{
    return const_iterator{TKey(), std::shared_ptr<Node>{}};
}

template <typename TKey, typename TData>
typename CPrefixTrie<TKey, TData>::const_iterator CPrefixTrie<TKey, TData>::begin() const
{
    return find(TKey());
}

template <typename TKey, typename TData>
typename CPrefixTrie<TKey, TData>::const_iterator CPrefixTrie<TKey, TData>::end() const
{
    return const_iterator{TKey(), std::shared_ptr<Node>{}};
}

using Key = std::string;
using Data = CClaimTrieData;
using Trie = CPrefixTrie<Key, Data>;
using iterator = Trie::iterator;
using const_iterator = Trie::const_iterator;

template class CPrefixTrie<Key, Data>;

template class Trie::Iterator<true>;
template class Trie::Iterator<false>;

template const_iterator& const_iterator::operator=<>(const iterator&) noexcept;

template iterator Trie::insert<>(const Key&, Data&);
template iterator Trie::insert<>(const Key&, Data&&);
template iterator Trie::insert<>(const Key&, const Data&);
template iterator Trie::insert<>(iterator&, const Key&, Data&);
template iterator Trie::insert<>(iterator&, const Key&, Data&&);
template iterator Trie::insert<>(iterator&, const Key&, const Data&);
