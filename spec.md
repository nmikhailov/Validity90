# Packets

## Init sequence

## Key exchange

1. Generate 0004 bytes of secure random data as RND1
2. Generate 001c bytes of secure random data as RND2
3. Send packet 140
	TLS v1.2 Client Hello
 
	```
	0000   		44 00 00 00 - unknown header
	0004 		16 03 03 00 43 01 00 00 3f 03 03 - TLS
	000f-0012   'RND1
	0012-002e	RND2
	002f   		07  - TLS
	0030   		00 00 00 00 00 00 00 00 04 c0 05 00 3d 00 00 0a  - TLS
	0040   		00 04 00 02 00 17 00 0b 00 02 01 00  - TLS
	```

	```
	Secure Sockets Layer
	    TLSv1.2 Record Layer: Handshake Protocol: Client Hello
	        Content Type: Handshake (22)
	        Version: TLS 1.2 (0x0303)
	        Length: 67
	        Handshake Protocol: Client Hello
	            Handshake Type: Client Hello (1)
	            Length: 63
	            Version: TLS 1.2 (0x0303)
	            Random
	                GMT Unix Time: 4 bytes as 'RND1
	                Random Bytes: 1c bytes as RND2
	            Session ID Length: 7
	            Session ID: 00000000000000
	            Cipher Suites Length: 4
	            Cipher Suites (2 suites)
	                Cipher Suite: TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA (0xc005)
	                Cipher Suite: TLS_RSA_WITH_AES_256_CBC_SHA256 (0x003d)
	            Compression Methods Length: 0
	            Extensions Length: 10
	            Extension: truncated_hmac
	                Type: truncated_hmac (0x0004)
	                Length: 2
	                Data (2 bytes)
	            Extension: ec_point_formats
	                Type: ec_point_formats (0x000b)
	                Length: 2
	                EC point formats Length: 1
	                Elliptic curves point formats (1)
	```


4. Receive Packet 130 as P002
	TLS v1.2 Server Hello, Certificate Request(custom), Server Hello Done

	Stable header: `16 03 03 00 3d 02 00 00 2d 03 03`


	```
	Secure Sockets Layer
	    TLSv1.2 Record Layer: Handshake Protocol: Multiple Handshake Messages
	        Content Type: Handshake (22)
	        Version: TLS 1.2 (0x0303)
	        Length: 61
	        Handshake Protocol: Server Hello
	            Handshake Type: Server Hello (2)
	            Length: 45
	            Version: TLS 1.2 (0x0303)
	            Random 	as SERVER_RAND
	            Session ID Length: 7
	            Session ID: 544c53900cb801
	            Cipher Suite: TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA (0xc005)
	            Compression Method: null (0)
	        Handshake Protocol: Certificate Request
	            Handshake Type: Certificate Request (13)
	            Length: 4
	            Certificate types count: 1
	            Certificate types (1 type)
	            Signature Hash Algorithms Length: 0
	            Distinguished Names Length: 3584
	            Distinguished Names (3584 bytes) - Invalid

	```
5. Copy 0020 bytes offset 000b-002a from P002 as SERVER_RAND
6. Gen EC P256 KeyPair as EPHEMERAL_KEY_ECDHE_DRV_PUB and EPHEMERAL_KEY_ECDHE_DRV_PRIV
7. Derive shared key from (EPHEMERAL_KEY_ECDHE_DRV_PRIV, STATIC_KEY_ECDH_DEV_PUB) as EPHEMERAL_KEY_RC2_A
8. Define CLIENT_AND_SERVER_RAND_WRAPPED

	```	
	0000 6b 65 79 20 65 78 70 61 6e 73 69 6f 6e 
	000d - 0010 = 'RND1
	0011 - 002c = RND2 
	002d - 003f = SERVER_RAND
	```

9. Define EPHEMERAL_KEY_RC2_B

	```
	HMAC(
		EPHEMERAL_KEY_RC2_A,
		HMAC(
			EPHEMERAL_KEY_RC2_A,
			HMAC(EPHEMERAL_KEY_RC2_A, SERVER_RAND)
		)
		+
		SERVER_RAND
	)
	```

10. Define EPHEMERAL_KEY_RC2_C

	```
	HMAC(
		SESSION_KEY_RC2_A,
		HMAC(SESSION_KEY_RC2_A, CLIENT_AND_SERVER_RAND_WRAPPED) 
		+ 
		CLIENT_AND_SERVER_RAND_WRAPPED
	)
	```
11. Define EPHEMERAL_KEY_AES_ENCRYPT

	```
	HMAC(
		EPHEMERAL_KEY_RC2_A,

		HMAC(
			EPHEMERAL_KEY_RC2_A,
			HMAC(
				EPHEMERAL_KEY_RC2_A,
				HMAC(EPHEMERAL_KEY_RC2_A, SERVER_RAND)
			)
		) + CLIENT_AND_SERVER_RAND_WRAPPED

	);
	```

12. Define EPHEMERAL_KEY_AES_DECRYPT

	```
	HMAC(EPHEMERAL_KEY_RC2_A,

		0000-001f 	HMAC(EPHEMERAL_KEY_RC2_A, HMAC(EPHEMERAL_KEY_RC2_A, HMAC(EPHEMERAL_KEY_RC2_A, HMAC(EPHEMERAL_KEY_RC2_A, SERVER_RAND))))
0020-006c	CLIENT_AND_SERVER_RAND_WRAPPED

	);
	```
13. Define PP1 
	HMAC(EPHEMERAL_KEY_RC2_A,

	)
	PP1_S = PP1[0:c]
		

14. Send packet 505
	TLS v1.2 Certificate(custom), Client Key Exchange, Certificate Verify(custom), Change chipher Spec, Encrypted handshake

	```
	// Unknown header
	0000   		44 00 00 00

	// TLS Handshake
	0004   		16 03 03 01

	// TLS Handshake - Certificate
	0008 		55 0b 00 00 c0 00 00 b8
	
	// Certificate(custom)
	0010   		00 00 b8
	0013-0014 	RND2[0:2]
	0015			           17 00 00 00 20 00 00 00 ab 9d fd
	0020   		ba 74 25 29 93 9d 2d 5d f4 77 ec 90 2e 13 b8 21
	0030   		1a 19 70 1e 50 2f f5 6e 6e 25 ae 8c 00 00 00 00
	0040   		00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
	0050   		00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
	0060   		00 dd f4 04 74 f0 7a e4 e0 79 d1 f1 9f ae bd a8
	0070   		ef 1e fa 18 c2 6a 76 ae a5 aa bf c3 4f 12 94 8c
	0080   		8f 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
	0090   		00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
	00a0   		00 00 00 00 00 00 00 00 00 00 00 00 00 a5 58 ed
	00b0   		0f 31 33 45 63 c8 8a d5 53 d9 e4 6e 20 5d 54 3b
	00c0   		83 99 cf 9b ef 9e a8 aa c5 eb fb 20 a2

	// TLS Handshake - Client Key exchange
	00cd		10 00 00
	00d0   		41 04 
	00d2-0112 	EPHEMERAL_KEY_ECDHE_DRV_PUB (no header)

	// TLS Handshake - Certificate Verify
	0112			  0f 00 00 48 30 46 02 21 00 a3 ad aa 61 00
	0120   		e6 9d bd cf 48 73 b7 a6 ed e3 62 0a 79 e4 f8 14
	0130   		27 4d eb 73 91 01 0c ae 08 b9 43 02 21 00 d3 28
	0140   		a4 86 cf 8b af 35 c9 04 f7 1f e2 56 22 f7 5d df
	0150   		53 13 4f c6 db 6b c0 0d 57 90 c4 23 fe 06 

	// TLS Change Chipher Spec
	015e   		14 03 03 00 01 01 - Change chipher

	// TLS Encrypted Handshake
	0164   		16 03 03 00 50 4b 77 62 ff a9 03 c1
	0170   		1e 6f d8 35 93 17 2d 54 ef 

	0179-01b8	Encrypt with EPHEMERAL_KEY_AES_ENCRYPT
		0000 		14 00 00 0c 
					da 04 57 9a 5d 22 ef 43 f2 b6 20 57 - unknown as PP1
		0010-002f  	HMAC (EPHEMERAL_KEY_RC2_C, 16 03 03 00 10 + PP1)	
		0030 		0f 0f 0f 0f 0f 0f 0f 0f 0f 0f 0f 0f 0f 0f 0f 0f  

	```
15. Receive packet

	```
	Secure Sockets Layer
	    TLSv1.2 Record Layer: Change Cipher Spec Protocol: Change Cipher Spec
	        Content Type: Change Cipher Spec (20)
	        Version: TLS 1.2 (0x0303)
	        Length: 1
	        Change Cipher Spec Message

	    TLSv1.2 Record Layer: Handshake Protocol: Encrypted Handshake Message
	        Content Type: Handshake (22)
	        Version: TLS 1.2 (0x0303)
	        Length: 80
	        Handshake Protocol: Encrypted Handshake Message
    ```


## Encrypted packet format

TLSv1.2 Application data

| Offset | Size | Name | Description |
|---|---|---|---|
| 0000  | 0003 | header | TLSv1.2 Data Header |
| 0003  | 0002 | $length | Packet length, can span across multiple USB_BULK packets |
| 0005  | $length | data | Data, encrypted with AES256 CBC mode, PKCS#5 padding |

# Appendix A: Static keys

## STATIC_KEY_ECDSA_DEV_PUB

```
BCRYPT_ECDSA_PUBLIC_P256_MAGIC 20

0000 f7 27 65 3b 4e 16 ce 06 65 a6 89 4d 7f 3a 30 d7
0010 d0 a0 be 31 0d 12 92 a7 43 67 1f df 69 f6 a8 d3 
0020 a8 55 38 f8 b6 be c5 0d 6e ef 8b d5 f4 d0 7a 88 
0030 62 43 c5 8b 23 93 94 8d f7 61 a8 47 21 a6 ca 94
```

## STATIC_KEY_ECDSA_DRV_PRIV
```
BCRYPT_ECDSA_PRIVATE_P256_MAGIC 20

0000 00 8c ae 25 6e 6e f5 2f 50 1e 70 19 1a 21 b8 13
0010 2e 90 ec 77 f4 5d 2d 9d 93 29 25 74 ba fd 9d ab 
0020 8f 8c 94 12 4f c3 bf aa a5 ae 76 6a c2 18 fa 1e 
0030 ef a8 bd ae 9f f1 d1 79 e0 e4 7a f0 74 04 f4 dd 
0040 d6 87 4c 06 b8 2d d1 ce dc 99 f9 2f b7 86 6c 20 
0050 09 72 23 68 b9 04 1f fa ff 72 de 8e 49 52 f5 94 
```

## STATIC_KEY_ECDH_DEV_PUB

```
BCRYPT_ECDH_PUBLIC_P256_MAGIC 20

0000 5f 71 17 6f 76 66 55 74 a3 86 53 53 10 f6 98 18 
0010 6f 42 9b f0 6e fa 05 9b 0c 3f 99 bc fe b5 d6 ce 
0020 3e 61 55 91 ab 00 99 b0 4f 6f 4b 68 ac bd 67 81 
0030 65 b8 26 75 1d 50 e3 87 d0 cc fd 49 5f f4 ce ca
```

## STATIC_KEY_AES_MASTER

```
0000 08 02 00 00 10 66 00 00 20 00 00 00 

0000 48 78 02 70 5e 5a c4 a9 93 1c 44 aa 4d 32 25 22
0010 39 e0 bf 8f 0c 85 4d de 49 0c cc f6 87 ef ad 9c 
```

## STATIC_KEY_RC2_MASTER
```
0000 08 02 00 00 02 66 00 00 20 00 00 00

0000 71 7c d7 2d 09 62 bc 4a 28 46 13 8d bb 2c 24 19 
0010 25 12 a7 64 07 06 5f 38 38 46 13 9d 4b ec 20 33 
```

## STATIC_KEY_RC2_MASTER2
```
0000 08 02 00 00 02 66 00 00 20 00 00 00 

0000 b7 01 5b e1 65 8f 48 d0 d3 95 4b 2c 79 fe 66 b5
0010 45 47 38 bd f3 a9 d4 ec e6 2e cf 7d d0 dd ba ba 
```
