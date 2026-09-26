# AtlasDevShowInventoryMenu

```text
AtlasDevShowInventoryMenu
Msg "READY TO GO?"
End
```

This instruction opens the same inventory menu as Select and pauses the script
until the player closes it. The regular controls and menu rules still apply.
Selecting equipment closes the category page. B closes the menu.

Use it in a normal dialogue script. When the menu closes, the dialogue frame
is rebuilt and the next instruction runs. Old dialogue text is cleared. Do not
call it from an NMI or frame-scheduler callback.

The original game blocks equipment changes inside buildings, including the
King's room. The menu still opens there, but equipment cannot be changed. Use
an outdoor script when the player needs to equip an item.

Enable it in a dense `iscript_opcodes` override using
`Mnemonic=AtlasDevShowInventoryMenu,Impl=AtlasDevShowInventoryMenu` at the next
free opcode byte. Its numeric opcode depends on that configuration.

The handler is 33 bytes. With its dispatch table, an otherwise empty extension
library uses 83 bytes. It adds no resident RAM or hooks. USA rev0, USA rev1,
Europe and Japan are supported.
