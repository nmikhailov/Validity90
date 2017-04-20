# Validity90

This project aims on reverse engineering protocol of Validity 138a:0090 fingerprint reader, creating specification and FLOSS libfprint driver.

## Discussions

Gitter chat: [![Gitter](https://img.shields.io/gitter/room/nwjs/nw.js.svg)](https://gitter.im/Validity90/Lobby?utm_source=share-link&utm_medium=link&utm_campaign=share-link)

libfrprint bug: [https://bugs.freedesktop.org/show_bug.cgi?id=94536](https://bugs.freedesktop.org/show_bug.cgi?id=94536)

Lenovo forums: [https://forums.lenovo.com/t5/Linux-Discussion/Validity-Fingerprint-Reader-Linux/td-p/3352145](https://forums.lenovo.com/t5/Linux-Discussion/Validity-Fingerprint-Reader-Linux/td-p/3352145)

## Notable files

1. [spec.md](spec.md) - Specification draft, the main work goes here right now.
2. [dissector.lua](dissector.lua) - Wireshark dissector for decrypting communication after key exchange.
3. [libfprint directory](libfprint) - libfprint repo with this driver integrated
4. [prototype](prototype/prototype.c) - Standalone prototype(extremly ugly code, would be completly rewritten for driver)

## Status
| 		Task 			| Specification/Analysis	| Prototype    | Driver 	 |
|-----------------------|---------------------------|--------------|-------------|
| Initialization  		| Done 					    | Done	 	   | Not Started |
| Crypto/Keys 			| Done 						| Done  	   | Not Started |
| TLS Handshake	        | Done              		| Done         | Not Started |
| Scan routine  		| In progress  				| In progress  | Not Started |
| Image format  		| In progress  				| In progress  | Not Started |
