# Validity90

This project aims on reverse engineering protocol of Validity 138a:0090, 138a:0091, 138a:0094, 138a:0097 fingerprint readers, creating specification and FLOSS libfprint driver.

## Discussions

Main chat of this project: [![Gitter](https://img.shields.io/gitter/room/nwjs/nw.js.svg)](https://gitter.im/Validity90/Lobby?utm_source=share-link&utm_medium=link&utm_campaign=share-link)

libfrprint issue: [https://bugs.freedesktop.org/show_bug.cgi?id=94536](https://bugs.freedesktop.org/show_bug.cgi?id=94536)
Lenovo forums: [https://forums.lenovo.com/t5/Linux-Discussion/Validity-Fingerprint-Reader-Linux/td-p/3352145](https://forums.lenovo.com/t5/Linux-Discussion/Validity-Fingerprint-Reader-Linux/td-p/3352145)

## Notable files

1. [spec.md](spec.md) - Specification draft, the main work goes here right now.
2. [dissector.lua](dissector.lua) - Wireshark dissector for decrypting communication after key exchange.
3. [libfprint directory](libfprint) - libfprint repo with this driver integrated
4. [prototype](prototype) - Standalone prototype(extremly ugly code, would be completly rewritten for driver)

## Testing

[Prototype](prototype) testers are needed, please share your result and join us in our [Gitter](https://gitter.im/Validity90/Lobby?utm_source=share-link&utm_medium=link&utm_campaign=share-link).

## Status
| 		      Task       			| Specification/Analysis  | Prototype   | Driver 	    |
|---------------------------|-------------------------|-------------|-------------|
| Initialization  		      | Done 					          | Done	 	    | Not Started |
| Configuration/Reconfig    | In progress 	          | In progress | Not Started |
| Pre TLS key exchange 	    | In progress 				    | Done        | Not Started |
| TLS 			                | Done 						        | Done  	    | Not Started |
| 90: Ops: scan, LED, etc| In progress  			      | Scan works  | Not Started |
| 97: Ops: enroll, check, reset, LED, etc| Not Started  			      | Not Started  | Not Started |
| Image format  		        | In progress  			      | Done        | Not Started |
| Image post-processing     | Not started             | Not started | Not Started |

## Donations

With paypal: [![Donate](https://img.shields.io/badge/Donate-PayPal-green.svg)](https://www.paypal.me/nmikhailov)  
With BTC: 1AsoCUGNVniBSBS359u2wKrA2CZiTgKZkM  
With ETH: 0x005D8d9b5609b9A3Ec8B3cBd463a949405A8d621  
