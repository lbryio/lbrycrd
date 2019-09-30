
#include <claimtrie.h>
#include <fs.h>
#include <lbry.h>
#include <limits>
#include <memory>
#include <prefixtrie.h>

#include <boost/interprocess/allocators/private_node_allocator.hpp>
#include <boost/interprocess/indexes/null_index.hpp>
#include <boost/interprocess/managed_mapped_file.hpp>

namespace bip = boost::interprocess;

typedef bip::basic_managed_mapped_file <
    char,
    bip::rbtree_best_fit<bip::null_mutex_family, bip::offset_ptr<void>>,
    bip::null_index
> managed_mapped_file;

template <typename T>
using node_allocator = bip::private_node_allocator<T, managed_mapped_file::segment_manager>;

static managed_mapped_file::segment_manager* segmentManager()
{
    struct CSharedMemoryFile
    {
        CSharedMemoryFile() : file(GetDataDir() / "shared.mem")
        {
            std::remove(file.c_str());
            auto size = (uint64_t)g_memfileSize * 1024ULL * 1024ULL * 1024ULL;
            menaged_file.reset(new managed_mapped_file(bip::create_only, file.c_str(), size));
        }
        ~CSharedMemoryFile()
        {
            menaged_file.reset();
            std::remove(file.c_str());
        }
        managed_mapped_file::segment_manager* segmentManager()
        {
            return menaged_file->get_segment_manager();
        }
        const fs::path file;
        std::unique_ptr<managed_mapped_file> menaged_file;
    };
    static CSharedMemoryFile shem;
    return shem.segmentManager();
}

template <typename T>
static node_allocator<T>& nodeAllocator()
{
    static node_allocator<T> allocator(segmentManager());
    return allocator;
}

template <typename T, class... Args>
static std::shared_ptr<T> nodeAllocate(Args&&... args)
{
    return std::allocate_shared<T>(nodeAllocator<T>(), std::forward<Args>(args)...);
}

template <typename T, class... Args>
static std::shared_ptr<T> allocateShared(Args&&... args)
{
    static auto allocate = g_memfileSize ? nodeAllocate<T, Args...> : std::make_shared<T, Args...>;
    try {
        return allocate(std::forward<Args>(args)...);
    }
    catch (const bip::bad_alloc&) {
        allocate = std::make_shared<T, Args...>; // in case we fill up the memfile
        LogPrint(BCLog::BENCH, "WARNING: The memfile is full; reverting to the RAM allocator for %s.\n", typeid(T).name());
        return allocate(std::forward<Args>(args)...);
    }
}

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
        stack.push_back(Bookmark{i.name, i.it, i.end});
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
        stack.emplace_back(Bookmark{name, it, children.end()});
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
typename CPrefixTrie<TKey, TData>::template Iterator<IsConst>::reference CPrefixTrie<TKey, TData>::Iterator<IsConst>::operator*()
{
    return reference{name, data()};
}

template <typename TKey, typename TData>
template <bool IsConst>
typename CPrefixTrie<TKey, TData>::template Iterator<IsConst>::const_reference CPrefixTrie<TKey, TData>::Iterator<IsConst>::operator*() const
{
    return const_reference{name, data()};
}

template <typename TKey, typename TData>
template <bool IsConst>
typename CPrefixTrie<TKey, TData>::template Iterator<IsConst>::pointer CPrefixTrie<TKey, TData>::Iterator<IsConst>::operator->()
{
    return &(data());
}

template <typename TKey, typename TData>
template <bool IsConst>
typename CPrefixTrie<TKey, TData>::template Iterator<IsConst>::const_pointer CPrefixTrie<TKey, TData>::Iterator<IsConst>::operator->() const
{
    return &(data());
}

template <typename TKey, typename TData>
template <bool IsConst>
const TKey& CPrefixTrie<TKey, TData>::Iterator<IsConst>::key() const
{
    return name;
}

template <typename TKey, typename TData>
template <bool IsConst>
typename CPrefixTrie<TKey, TData>::template Iterator<IsConst>::data_reference CPrefixTrie<TKey, TData>::Iterator<IsConst>::data()
{
    auto shared = node.lock();
    assert(shared);
    return *(shared->data);
}

template <typename TKey, typename TData>
template <bool IsConst>
const TData& CPrefixTrie<TKey, TData>::Iterator<IsConst>::data() const
{
    auto shared = node.lock();
    assert(shared);
    return *(shared->data);
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
template <typename TIterator, typename TNode>
std::vector<TIterator> CPrefixTrie<TKey, TData>::nodes(const TKey& key, TNode root)
{
    std::vector<TIterator> ret;
    ret.reserve(1 + key.size());
    ret.emplace_back(TKey{}, root);
    if (key.empty()) return ret;
    TKey name;
    using CBType = callback<TNode>;
    CBType cb = [&name, &ret](const TKey& key, TNode node) {
        name.insert(name.end(), key.begin(), key.end());
        ret.emplace_back(name, node);
    };
    find(key, root, cb);
    return ret;
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
        it = children.emplace(key, allocateShared<Node>()).first;
        return it->second;
    }
    if (count < it->first.size()) {
        TKey prefix(key.begin(), key.begin() + count);
        TKey postfix(it->first.begin() + count, it->first.end());
        auto nodes = std::move(it->second);
        children.erase(it);
        ++size;
        it = children.emplace(std::move(prefix), allocateShared<Node>()).first;
        it->second->children.emplace(std::move(postfix), std::move(nodes));
        if (key.size() == count)
            return it->second;
        it->second->data = allocateShared<TData>();
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

    nodes.back().second->data = allocateShared<TData>();
    for (; nodes.size() > 1; nodes.pop_back()) {
        // if we have only one child and no data ourselves, bring them up to our level
        auto& cNode = nodes.back().second;
        auto onlyOneChild = cNode->children.size() == 1;
        auto noData = cNode->data->empty();
        if (onlyOneChild && noData) {
            auto child = cNode->children.begin();
            auto& prefix = nodes.back().first;
            auto newKey = prefix;
            auto& postfix = child->first;
            newKey.insert(newKey.end(), postfix.begin(), postfix.end());
            auto& pNode = nodes[nodes.size() - 2].second;
            pNode->children.emplace(std::move(newKey), std::move(child->second));
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
CPrefixTrie<TKey, TData>::CPrefixTrie() : size(0), root(allocateShared<Node>())
{
    root->data = allocateShared<TData>();
}

template <typename TKey, typename TData>
template <typename TDataUni>
typename CPrefixTrie<TKey, TData>::iterator CPrefixTrie<TKey, TData>::insert(const TKey& key, TDataUni&& data)
{
    auto& node = key.empty() ? root : insert(key, root);
    node->data = allocateShared<TData>(std::forward<TDataUni>(data));
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
        copy = iterator{std::move(name), node};
    }
    copy.node.lock()->data = allocateShared<TData>(std::forward<TDataUni>(data));
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
std::vector<typename CPrefixTrie<TKey, TData>::iterator> CPrefixTrie<TKey, TData>::nodes(const TKey& key)
{
    if (empty()) return {};
    return nodes<iterator>(key, root);
}

template <typename TKey, typename TData>
std::vector<typename CPrefixTrie<TKey, TData>::const_iterator> CPrefixTrie<TKey, TData>::nodes(const TKey& key) const
{
    if (empty()) return {};
    return nodes<const_iterator>(key, root);
}

template <typename TKey, typename TData>
bool CPrefixTrie<TKey, TData>::erase(const TKey& key)
{
    auto size_was = height();
    if (key.empty()) {
        root->data = allocateShared<TData>();
    } else {
        erase(key, root);
    }
    return size_was != height();
}

template <typename TKey, typename TData>
void CPrefixTrie<TKey, TData>::clear()
{
    size = 0;
    root->data = allocateShared<TData>();
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
    return size + (root->data->empty() ? 0 : 1);
}

template <typename TKey, typename TData>
typename CPrefixTrie<TKey, TData>::iterator CPrefixTrie<TKey, TData>::begin()
{
    return find(TKey());
}

template <typename TKey, typename TData>
typename CPrefixTrie<TKey, TData>::iterator CPrefixTrie<TKey, TData>::end()
{
    return {};
}

template <typename TKey, typename TData>
typename CPrefixTrie<TKey, TData>::const_iterator CPrefixTrie<TKey, TData>::cbegin()
{
    return find(TKey());
}

template <typename TKey, typename TData>
typename CPrefixTrie<TKey, TData>::const_iterator CPrefixTrie<TKey, TData>::cend()
{
    return {};
}

template <typename TKey, typename TData>
typename CPrefixTrie<TKey, TData>::const_iterator CPrefixTrie<TKey, TData>::begin() const
{
    return find(TKey());
}

template <typename TKey, typename TData>
typename CPrefixTrie<TKey, TData>::const_iterator CPrefixTrie<TKey, TData>::end() const
{
    return {};
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
