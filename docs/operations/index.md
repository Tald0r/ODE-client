# Runbooks

* [Running the client](running-the-client.md) - DarkEden.exe runs from the Visual Studio debugger and is not launched by an agent unless asked; an unlisted DLL beside it exits -1 before main, resources are read loose first and then from the .rpk, and off Windows the headless run reaches the main menu.

# Reference

* [Resource text encoding](resource-text-encoding.md) - Resource records decode to UTF-8 from the pack's configured code page and encode back to it on save; XML and BOM-marked text override the pack default, and the XML parser's limits reject a hostile document without touching the loaded tree.
