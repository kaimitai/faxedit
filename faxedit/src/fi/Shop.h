#ifndef FI_SHOP_H
#define FI_SHOP_H

#include <vector>

using byte = unsigned char;

namespace fi {

	struct ShopEntry {
		byte m_item;
		int m_price;
	};

	struct Shop {
		std::vector<fi::ShopEntry> m_entries;

		Shop(void) = default;
		void add_entry(byte p_item, uint16_t p_price);
		void add_entry(byte p_item, byte p_lo, byte p_hi);

		std::vector<byte> to_bytes(void) const;
		std::size_t byte_size(void) const;
	};

}

#endif
