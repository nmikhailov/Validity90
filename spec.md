# Initialisation

## Static init sequence

1. Send INIT_MSG1, receive INIT_RSP1
2. Send INIT_MSG2, receive INIT_RSP2
3. Send INIT_MSG3, receive INIT_RSP3
4. Send INIT_MSG4, receive INIT_RSP4
5. Send INIT_MSG5, receive INIT_RSP5
6. Send INIT_MSG6, receive INIT_RSP6

## Key derivation

Sym key1:
0x20 bytes

First 8 bytes:  
71 7c d7 2d 09 62 bc 4a
Second 8 bytes
28 46 13 8d bb 2c 24 19 
Third

0000 71 7c d7 2d 09 62 bc 4a   28 46 13 8d bb 2c 24 19 
0010 25 12 a7 64 07 06 5f 38   38 46 13 9d 4b ec 20 33 


0000 3a 4c 76 b7 6a 97 98 1d   12 74 24 7e 16 66 10 e7
0010 7f 4d 9c 9d 07 d3 c7 28   e5 32 91 6b dd 28 b4 54

also static deviation
b7 01 5b e1 
0010 65 8f 48 d0 d3 95 4b 2c 79 fe 66 b5 45 47 38 bd 
0020 f3 a9 d4 ec e6 2e cf 7d d0 dd ba ba 

## TLS Handshake

Modified [TLS 1.2](https://tools.ietf.org/html/rfc5246) is used as cryptographic protocol.

### Difference from TLS 1.2

1. Client handshake messages are prefixed with 4 byte prefix: 44 00 00 00
2. CertificateRequest and Certificate are in unknown format
3. Certificate Verify message has no algorithm bytes
4. Device doesn't allow much variation(ie extra extension in ClientHello) in handshake.
5. Custom padding scheme for AES encryption is used - almost like PKCS#8, but all values are one off, eg: 00; 01 01; 02 02 02


### Handshake session
1. Client Hello
 
	```
	0000   		44 00 00 00
	0004 		16 03 03 00 43 01 00 00 3f 03 03
	000f-002e	Random
	002f   		07
	0030   		00 00 00 00 00 00 00 00 04 c0 05 00 3d 00 00 0a
	0040   		00 04 00 02 00 17 00 0b 00 02 01 00
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
	            Random 0x20 bytes
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

2. Server Hello, Certificate Request(custom), Server Hello Done

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
	            Random
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
3. Certificate(custom), Client Key Exchange, Certificate Verify(custom), Change chipher Spec, Encrypted finished

	```
	0000   		44 00 00 00
	0004   		16 03 03 01

	// TLS Handshake - Certificate
	0008 		55 0b 00 00 c0 00 00 b8
	
	// Certificate(custom)
	0010   		00 00 b8
	0013-0014 	client_random[4:6]
	0015-00c0	DATA_CERT

	// TLS Handshake - Client Key exchange
	00cd		10 00 00
	00d0   		41 04 
	00d2-0112 	EPHEMERAL_KEY_ECDHE_DRV_PUB

	// TLS Handshake - Certificate Verify
	0112-015e

	// TLS Change Chipher Spec
	015e   		14 03 03 00 01 01

	// TLS Encrypted Handshake
	0164   		16 03 03 00 50 4b 77 62 ff a9 03 c1
	0170   		1e 6f d8 35 93 17 2d 54 ef 

	0179-01b8

	```
    
    ```
    TLSv1.2 Record Layer: Handshake Protocol: Multiple Handshake Messages
        Content Type: Handshake (22)
        Version: TLS 1.2 (0x0303)
        Length: 341
        Handshake Protocol: Certificate
            Handshake Type: Certificate (11)
            Length: 192
            Certificates Length: 184
        Handshake Protocol: Client Key Exchange
            Handshake Type: Client Key Exchange (16)
            Length: 65
            EC Diffie-Hellman Client Params
        Handshake Protocol: Certificate Verify
            Handshake Type: Certificate Verify (15)
            Length: 72
            Signature Hash Algorithm: 0x3046
                Signature Hash Algorithm Hash: Unknown (48)
                Signature Hash Algorithm Signature: Unknown (70)
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
    
4. Change Cipher Spec, Finish

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


# Appendix A: Static keys

## STATIC_KEY_ECDSA_DEV_PUB

```
BCRYPT_ECDSA_PUBLIC_P256_MAGIC 20

0000 f7 27 65 3b 4e 16 ce 06 65 a6 89 4d 7f 3a 30 d7
0010 d0 a0 be 31 0d 12 92 a7 43 67 1f df 69 f6 a8 d3 
0020 a8 55 38 f8 b6 be c5 0d 6e ef 8b d5 f4 d0 7a 88 
0030 62 43 c5 8b 23 93 94 8d f7 61 a8 47 21 a6 ca 94
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

# Appendix B: Initialisation sequence

## INIT_MSG1
```
0000 01 
```

## INIT_RSP1
```
0000 00 00 f0 b0 5e 54 a4 00  00 00 06 07 01 30 00 01 
0010 00 00 26 85 88 42 45 3b  00 23 00 00 00 00 01 00 
0020 00 00 00 00 00 07 
```

## INIT_MSG2
```
0000 19 
```

## INIT_RSP2
```
0000 00 00 00 03 01 02 00 01  00 00 00 00 00 00 00 00 
0010 00 00 00 00 01 01 01 01  00 00 00 00 02 02 02 02 
0020 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 
0030 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 
0040 00 00 00 00 
```

## INIT_MSG3
```
0000 43 02 
```

## INIT_RSP3
```
0000 00 00 01 00 00 00 06 00  98 c9 71 56 01 00 34 46 
0010 02 00 07 00 60 39 00 00  01 00 84 08 01 00 07 00 
0020 b0 02 00 00 02 00 84 28  03 00 12 00 10 10 00 00 
0030 02 00 66 37 01 00 0c 00  40 22 02 00 01 00 86 47 
0040 00 00 01 00 20 5a 00 00  02 00 23 77 00 00 01 00 
0050 d0 2f 00 00 
```

## INIT_MSG4
```
0000 06 02 00 00 01 39 17 b3  dd a9 13 83 b5 bc ac 64 
0010 fa 4a d3 5d ce 96 57 0a  9d 2d 97 4b 80 92 6a 43 
0020 1f 9c d4 62 48 98 0a 26  3c 6f ce f6 a8 28 39 a9 
0030 0b 59 ac 59 08 48 85 9a  fa c8 17 b7 d5 3b f5 1c 
0040 d3 20 5c 1b 8f 43 04 8b  e8 25 3c 3b d2 47 93 7c 
0050 83 7a ca 8b 18 d3 cc 8e  e8 c8 97 1a c4 f6 88 81 
0060 3c f3 d8 55 0d 71 49 69  85 b7 ec 07 ff 2d c7 89 
0070 6d 33 0f da b2 63 a0 ee  43 3a 5c 4b c9 10 43 9d 
0080 1c 61 61 85 3f eb 03 f5  50 22 09 50 2e 73 08 be 
0090 b7 91 94 73 cf e6 9f 42  2c 30 50 2d 22 6a 4d 0a 
00a0 34 d9 6c 8c 77 95 6c f6  9d b8 ef 6c f9 27 a3 b5 
00b0 78 49 d4 aa 8a d4 b4 42  66 92 3e 34 b8 2a 39 c8 
00c0 14 6b a3 cd 70 8c 70 df  ed b5 0c 2d e6 1f eb 45 
00d0 b1 d4 f1 95 84 29 72 03  f5 fd c8 65 79 5f ec 9d 
00e0 64 49 f3 ba 9b 6f 1e 4b  ed 69 8e e1 51 e8 3d 4d 
00f0 87 02 f7 6a 40 06 cf a2  4d 9b 79 78 88 20 3b 22 
0100 69 f8 a7 7d 52 40 34 ac  32 e4 af 58 b8 6e bc 63 
0110 55 2c b3 5b 12 b2 85 25  5d ea f3 a3 2b f4 6c dc 
0120 5a d3 bc 1c 9e d1 bc c1  12 c7 21 43 f9 ae c5 68 
0130 e2 ca cf a8 9b a0 c7 bb  65 59 0d 8b 93 e6 87 1a 
0140 33 c6 c6 98 3c 0a cd 04  e7 37 ff 55 ee e0 24 ca 
0150 6b 9a 48 33 2c 1a 69 a5  a3 fd d2 4b 96 4c f7 e7 
0160 c5 52 29 bb 0b 48 a6 e3  39 eb 2c 42 d0 7e c8 50 
0170 a4 ee 78 06 60 ad 6c 77  ff a3 02 a6 3b d1 94 26 
0180 13 4c 45 33 d6 f9 67 44  11 63 fb 78 b7 35 47 c6 
0190 8a 49 3b 2f 80 0d 3c da  b8 27 b1 16 76 27 89 99 
01a0 2a ae 3c 8a b3 45 a4 9e  dd 31 2d fd 2a 27 bc 50 
01b0 14 27 dc 7f a0 0a c3 c5  c3 65 51 db b3 d5 ca d8 
01c0 d5 bd 7c ea 37 e5 8a 31  30 7a 6d 50 e6 ae 37 9a 
01d0 53 f1 36 66 78 c0 74 1a  3d 87 2b 8d cf ef a7 f6 
01e0 31 28 dc 82 45 
```

## INIT_RSP4
```
0000 00 00 
```

## INIT_MSG5
```
0000 3e 
```

## INIT_RSP5
```
0000 00 00 ef 00 40 00 00 10  01 00 00 01 01 00 05 00 
0010 01 04 07 00 00 10 00 00  00 10 00 00 02 01 02 00 
0020 00 20 00 00 00 e0 03 00  05 05 03 00 00 00 04 00 
0030 00 80 00 00 06 06 03 00  00 80 04 00 00 80 00 00 
0040 04 03 05 00 00 00 05 00  00 00 03 00 
```

## INIT_MSG6
```
0000 40 01 01 00 00 00 00 00  00 00 10 00 00 
```

## INIT_RSP6
```
0000 00 00 00 10 00 00 00 00  00 00 01 00 6e 34 0b 9c 
0010 ff b3 7a 98 9c a5 44 e6  bb 78 0a 2c 78 90 1d 3f 
0020 b3 37 38 76 85 11 a3 06  17 af a0 1d 00 04 00 a1 
0030 00 c8 38 d8 e1 db f5 04  53 04 1a c5 a7 b4 0b 2f 
0040 1e f2 7d 7e 1b fd 48 da  a9 42 06 59 f3 3b 07 a7 
0050 e3 02 65 4c 1a dd a3 57  65 13 84 c7 98 38 4e 5e 
0060 d9 c7 33 5c ed 15 55 3c  f5 f4 de 14 a0 f2 59 68 
0070 00 a2 a0 98 58 c2 06 67  d5 c1 06 e3 bf e6 6a ec 
0080 6a c0 2d b2 d8 77 d9 0e  c4 12 e3 ab 48 ab aa b4 
0090 b9 56 75 30 69 9d 0a c3  d9 bb ff de 42 11 bd 34 
00a0 03 21 cf a2 8d 3c 1b e4  ba f0 1f f4 40 69 6f b4 
00b0 78 18 f3 2d 6b 22 80 86  64 31 14 34 2a 81 2c cc 
00c0 d7 c6 62 f3 9e 5f 78 a6  39 d3 db 57 c3 30 d4 dd 
00d0 12 8f 12 90 7e 4b 95 09  0e fa a2 e3 17 07 e9 74 
00e0 d8 33 a2 42 20 00 9a 33  ca 70 1c b9 3f 02 6e 78 
00f0 a2 ca 03 00 b8 00 ed 52  bb 71 b3 d9 0c 00 86 ad 
0100 64 0d 45 76 c7 32 b6 d5  d3 39 2d 89 5e 65 4b 60 
0110 6a 82 6a e5 bd 0c 17 00  00 00 20 00 00 00 ab 9d 
0120 fd ba 74 25 29 93 9d 2d  5d f4 77 ec 90 2e 13 b8 
0130 21 1a 19 70 1e 50 2f f5  6e 6e 25 ae 8c 00 00 00 
0140 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 
0150 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 
0160 00 00 dd f4 04 74 f0 7a  e4 e0 79 d1 f1 9f ae bd 
0170 a8 ef 1e fa 18 c2 6a 76  ae a5 aa bf c3 4f 12 94 
0180 8c 8f 00 00 00 00 00 00  00 00 00 00 00 00 00 00 
0190 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 
01a0 00 00 00 00 00 00 00 00  00 00 00 00 00 00 a5 58 
01b0 ed 0f 31 33 45 63 c8 8a  d5 53 d9 e4 6e 20 5d 54 
01c0 3b 83 99 cf 9b ef 9e a8  aa c5 eb fb 20 a2 05 00 
01d0 a4 01 ec 5d 90 0e 5a 79  58 6d 2c db ee c6 22 40 
01e0 c6 89 9d 37 47 5e 0f 46  bb 9e fd 3f 5a 4f 32 e8 
01f0 27 d2 17 00 00 00 00 01  00 00 01 00 00 00 fc ff 
0200 ff ff ff ff ff ff ff ff  ff ff 00 00 00 00 00 00 
0210 00 00 00 00 00 00 01 00  00 00 ff ff ff ff 00 00 
0220 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 
0230 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 
0240 00 00 4b 60 d2 27 3e 3c  ce 3b f6 b0 53 cc b0 06 
0250 1d 65 bc 86 98 76 55 bd  eb b3 e7 93 3a aa d8 35 
0260 c6 5a 00 00 00 00 00 00  00 00 00 00 00 00 00 00 
0270 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 
0280 00 00 00 00 00 00 96 c2  98 d8 45 39 a1 f4 a0 33 
0290 eb 2d 81 7d 03 77 f2 40  a4 63 e5 e6 bc f8 47 42 
02a0 2c e1 f2 d1 17 6b 00 00  00 00 00 00 00 00 00 00 
02b0 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 
02c0 00 00 00 00 00 00 00 00  00 00 f5 51 bf 37 68 40 
02d0 b6 cb ce 5e 31 6b 57 33  ce 2b 16 9e 0f 7c 4a eb 
02e0 e7 8e 9b 7f 1a fe e2 42  e3 4f 00 00 00 00 00 00 
02f0 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 
0300 00 00 00 00 00 00 00 00  00 00 00 00 00 00 51 25 
0310 63 fc c2 ca b9 f3 84 9e  17 a7 ad fa e6 bc ff ff 
0320 ff ff ff ff ff ff 00 00  00 00 ff ff ff ff 00 00 
0330 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 
0340 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 
0350 00 00 ff ff ff ff ff ff  ff ff ff ff ff ff 00 00 
0360 00 00 00 00 00 00 00 00  00 00 01 00 00 00 ff ff 
0370 ff ff 00 00 00 00 00 00  00 00 00 00 00 00 00 00 
0380 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 
0390 00 00 00 00 00 00 01 00  00 01 53 41 e6 b2 64 69 
03a0 79 a7 0e 57 65 30 07 a1  f3 10 16 94 21 ec 9b dd 
03b0 9f 1a 56 48 f7 5a de 00  5a f1 00 00 00 00 00 00 
03c0 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 
03d0 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 
03e0 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 
03f0 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 
0400 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 
0410 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 
0420 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 
0430 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 
0440 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 
0450 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 
0460 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 
0470 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 
0480 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 
0490 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 
04a0 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 
04b0 00 00 00 00 00 00 00 00  00 00 02 00 00 01 53 41 
04c0 e6 b2 64 69 79 a7 0e 57  65 30 07 a1 f3 10 16 94 
04d0 21 ec 9b dd 9f 1a 56 48  f7 5a de 00 5a f1 00 00 
04e0 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 
04f0 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 
0500 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 
0510 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 
0520 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 
0530 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 
0540 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 
0550 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 
0560 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 
0570 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 
0580 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 
0590 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 
05a0 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 
05b0 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 
05c0 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 
05d0 00 00 00 00 00 00 00 00  00 00 00 00 00 00 06 00 
05e0 90 01 d7 b7 f6 53 2b f4  a3 4f 4f 41 90 fe ad 55 
05f0 1c e6 2a ba 54 08 e5 30  60 e6 36 1c 35 6a 77 1d 
0600 c7 7b 20 00 00 00 17 00  00 00 ce d6 b5 fe bc 99 
0610 3f 0c 9b 05 fa 6e f0 9b  42 6f 18 98 f6 10 53 53 
0620 86 a3 74 55 66 76 6f 17  71 5f 00 00 00 00 00 00 
0630 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 
0640 00 00 00 00 00 00 00 00  00 00 00 00 00 00 ca ce 
0650 f4 5f 49 fd cc d0 87 e3  50 1d 75 26 b8 65 81 67 
0660 bd ac 68 4b 6f 4f b0 99  00 ab 91 55 61 3e 00 00 
0670 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 
0680 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 
0690 00 00 48 00 00 00 30 46  02 21 00 92 a1 f8 3a d4 
06a0 45 57 cb 82 0f 2f 07 0f  af 87 e5 1c 82 9d 85 29 
06b0 28 ab 9e aa 0d 23 31 9e  a8 25 5e 02 21 00 8d 98 
06c0 5c ba 0c 62 39 a5 31 cf  20 c0 14 a9 57 29 b7 62 
06d0 d7 75 5a d6 8c f8 20 dd  93 f6 45 a0 59 53 00 00 
06e0 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 
06f0 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 
0700 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 
0710 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 
0720 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 
0730 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 
0740 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 
0750 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 
0760 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 
0770 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 
0780 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 
0790 00 00 ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
07a0 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
07b0 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
07c0 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
07d0 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
07e0 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
07f0 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0800 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0810 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0820 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0830 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0840 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0850 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0860 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0870 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0880 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0890 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
08a0 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
08b0 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
08c0 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
08d0 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
08e0 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
08f0 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0900 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0910 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0920 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0930 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0940 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0950 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0960 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0970 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0980 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0990 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
09a0 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
09b0 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
09c0 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
09d0 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
09e0 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
09f0 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0a00 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0a10 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0a20 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0a30 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0a40 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0a50 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0a60 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0a70 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0a80 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0a90 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0aa0 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0ab0 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0ac0 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0ad0 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0ae0 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0af0 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0b00 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0b10 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0b20 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0b30 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0b40 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0b50 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0b60 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0b70 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0b80 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0b90 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0ba0 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0bb0 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0bc0 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0bd0 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0be0 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0bf0 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0c00 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0c10 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0c20 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0c30 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0c40 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0c50 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0c60 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0c70 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0c80 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0c90 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0ca0 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0cb0 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0cc0 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0cd0 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0ce0 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0cf0 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0d00 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0d10 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0d20 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0d30 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0d40 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0d50 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0d60 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0d70 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0d80 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0d90 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0da0 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0db0 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0dc0 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0dd0 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0de0 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0df0 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0e00 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0e10 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0e20 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0e30 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0e40 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0e50 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0e60 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0e70 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0e80 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0e90 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0ea0 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0eb0 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0ec0 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0ed0 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0ee0 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0ef0 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0f00 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0f10 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0f20 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0f30 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0f40 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0f50 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0f60 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0f70 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0f80 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0f90 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0fa0 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0fb0 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0fc0 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0fd0 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0fe0 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
0ff0 ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff 
1000 ff ff ff ff ff ff ff ff 
```