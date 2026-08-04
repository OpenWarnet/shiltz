#include "Map.h"

#include <algorithm>

void Map::AddItem(Item item)
{
    m_items.push_back(item);
}

bool Map::RemoveItem(std::uint32_t id)
{
    auto it = std::find_if(m_items.begin(), m_items.end(),
                            [id](const Item& item) { return item.id == id; });
    if (it == m_items.end())
        return false;

    m_items.erase(it);
    return true;
}

const std::vector<Item>& Map::Items() const
{
    return m_items;
}
