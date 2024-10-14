ReTI-Code
---------

A simple emulator for the ReTI processor.

- `asreti` assembler (ReTI assembler into ReTI code)
- `decbin` decodes binary (code/data) into hexadecimal
- `disreti` dissambler (ReTI code to ReTI assembler)
- `emreti` emulator runs ReTI code
- `enchex` encode hexadecimal data into binary
- `ranreti` generates random assember program
- `retiquiz` interactive quiz on machine code

To configure, build and test run `./configure && make test`.

The following is an example of generating and running a simple program:
```
$ ./ranreti 1910466996612083206 4
; ranreti 1910466996612083206 4
STOREIN2 2581947      ; 00000000 a32765bb
STOREIN1 15065599     ; 00000001 99e5e1ff
OPLUSI ACC 0xbc4285   ; 00000002 13bc4285
STOREIN2 3521395      ; 00000003 af35bb73
$ ./ranreti 1910466996612083206 4 | ./asreti | ./decbin
00000000 a02765bb
00000001 90e5e1ff
00000002 13bc4285
00000003 a035bb73
$
$ ./ranreti 1910466996612083206 4 | ./asreti | ./disreti
STOREIN2 2581947      ; 00000000 a02765bb
STOREIN1 15065599     ; 00000001 90e5e1ff
OPLUSI ACC 0xbc4285   ; 00000002 13bc4285
STOREIN2 3521395      ; 00000003 a035bb73
$ ./ranreti 1910466996612083206 4 | ./asreti | ./emreti
002765bb 00000000
0035bb73 00bc4285
00e5e1ff 00000000
```
Specifying `-s` (or `--step`) prints all the individuell executed
instructions and their effect:
```
$ ./ranreti 1910466996612083206 4 | ./asreti | ./emreti -s
STEPS    PC       CODE     IN1      IN2      ACC      INSTRUCTION         ACTION
1        00000000 a02765bb 00000000 00000000 00000000 STOREIN2 2581947    M(0x2765bb) = M(<IN2> + <0x2765bb>) = M(0x0 + 0x2765bb) = ACC = 0
2        00000001 90e5e1ff 00000000 00000000 00000000 STOREIN1 15065599   M(0xe5e1ff) = M(<IN1> + <0xe5e1ff>) = M(0x0 + 0xe5e1ff) = ACC = 0
3        00000002 13bc4285 00000000 00000000 00000000 OPLUSI ACC 0xbc4285 ACC = ACC ^ 0xbc4285 = 0x0 ^ 0xbc4285 = 0xbc4285
4        00000003 a035bb73 00000000 00000000 00bc4285 STOREIN2 3521395    M(0x35bb73) = M(<IN2> + <0x35bb73>) = M(0x0 + 0x35bb73) = ACC = bc4285
5        00000004 ........ 00000000 00000000 00bc4285 <undefined>
ADDRESS  DATA     BYTES       ASCII  UNSIGNED       SIGNED
002765bb 00000000 00 00 00 00 ....          0            0
0035bb73 00bc4285 85 42 bc 00 .B..   12337797     12337797
00e5e1ff 00000000 00 00 00 00 ....          0            0
```
For more information on using these tools use their command line option `-h`.
