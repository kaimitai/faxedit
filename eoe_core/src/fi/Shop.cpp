#include "Shop.h"

void fi::Shop::add_entry(byte p_item, uint16_t p_price) {
	m_entries.push_back(fi::ShopEntry(p_item, p_price));
}

void fi::Shop::add_entry(byte p_item, byte p_lo, byte p_hi) {
	m_entries.push_back(fi::ShopEntry(p_item,
		static_cast<uint16_t>(p_lo) + 256 * static_cast<uint16_t>(p_hi)
	));
}

std::vector<byte> fi::Shop::to_bytes(void) const {
	std::vector<byte> result;

	for (const auto& entry : m_entries) {
		result.push_back(entry.m_item);
		result.push_back(static_cast<byte>(entry.m_price % 256));
		result.push_back(static_cast<byte>(entry.m_price / 256));
	}

	// delimiter
	result.push_back(0xff);

	return result;
}

std::size_t fi::Shop::byte_size(void) const {
	// 3 bytes per entry and a final delimiter
	return 3 * m_entries.size() + 1;
}
