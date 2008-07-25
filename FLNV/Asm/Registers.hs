module FLNV.Asm.Registers ( Width(..), bitWidth,
                            Register(..), registerWidth) where

import Data.Array

{- Public interface -- width constants, and the registers -}

data Width = BYTE | WORD | LONG | QUADWORD

bitWidth :: Width -> Integer
bitWidth BYTE = 8
bitWidth WORD = 16
bitWidth LONG = 32
bitWidth QUADWORD = 64

data Register = AL  | AH  | BL   | BH   | CL   | CH   | DL   | DH
              | R8B | R9B | R10B | R11B | R12B | R13B | R14B | R15B
              | SPL | BPL | DIL  | SIL
              -- 16-bit GPRs
              | AX  | BX  | CX   | DX   | SP   | BP   | DI   | SI
              | R8W | R9W | R10W | R11W | R12W | R13W | R14W | R15W
              -- 32-bit GPRs 
              | EAX | EBX | ECX  | EDX  | ESP  | EBP  | EDI  | ESI
              | R8D | R9D | R10D | R11D | R12D | R13D | R14D | R15D
              -- 64-bit GPRs
              | RAX | RBX | RCX  | RDX  | RSP  | RBP  | RDI  | RSI
              | R8  | R9  | R10  | R11  | R12  | R13  | R14  | R15
              -- Instruction pointer
              | RIP
                deriving (Enum, Eq, Ord, Ix)

instance Show Register where
    show = show . opsName . (regOps!)

registerWidth :: Register -> Width
registerWidth = opsWidth . (regOps!)

{- Internal bookkeeping -}

data RegOps = R String Width

opsName :: RegOps -> String
opsName (R n _) = n

opsWidth :: RegOps -> Width
opsWidth (R _ w) = w

regOps :: Array Register RegOps
regOps = array (AL, RIP)
          [ (AL  , R "%al"   BYTE)
          , (AH  , R "%ah"   BYTE)
          , (BL  , R "%bl"   BYTE)
          , (BH  , R "%bh"   BYTE)
          , (CL  , R "%cl"   BYTE)
          , (CH  , R "%ch"   BYTE)
          , (DL  , R "%dl"   BYTE)
          , (DH  , R "%dh"   BYTE)
          , (R8B , R "%r8b"  BYTE)
          , (R9B , R "%r9b"  BYTE)
          , (R10B, R "%r10b" BYTE)
          , (R11B, R "%r11b" BYTE)
          , (R12B, R "%r12b" BYTE)
          , (R13B, R "%r13b" BYTE)
          , (R14B, R "%r14b" BYTE)
          , (R15B, R "%r15b" BYTE)
          , (SPL , R "%spl"  BYTE)
          , (BPL , R "%bpl"  BYTE)
          , (DIL , R "%dil"  BYTE)
          , (SIL , R "%sil"  BYTE)
          , (AX  , R "%ax"   WORD)
          , (BX  , R "%bx"   WORD)
          , (CX  , R "%cx"   WORD)
          , (DX  , R "%dx"   WORD)
          , (SP  , R "%sp"   WORD)
          , (BP  , R "%bp"   WORD)
          , (DI  , R "%di"   WORD)
          , (SI  , R "%si"   WORD)
          , (R8W , R "%r8w"  WORD)
          , (R9W , R "%r9w"  WORD)
          , (R10W, R "%r10w" WORD)
          , (R11W, R "%r11w" WORD)
          , (R12W, R "%r12w" WORD)
          , (R13W, R "%r13w" WORD)
          , (R14W, R "%r14w" WORD)
          , (R15W, R "%r15w" WORD)
          , (EAX , R "%eax"  LONG)
          , (EBX , R "%ebx"  LONG)
          , (ECX , R "%ecx"  LONG)
          , (EDX , R "%edx"  LONG)
          , (ESP , R "%esp"  LONG)
          , (EBP , R "%ebp"  LONG)
          , (EDI , R "%edi"  LONG)
          , (ESI , R "%esi"  LONG)
          , (R8D , R "%r8d"  LONG)
          , (R9D , R "%r9d"  LONG)
          , (R10D, R "%r10d" LONG)
          , (R11D, R "%r11d" LONG)
          , (R12D, R "%r12d" LONG)
          , (R13D, R "%r13d" LONG)
          , (R14D, R "%r14d" LONG)
          , (R15D, R "%r15d" LONG)
          , (RAX , R "%rax" QUADWORD)
          , (RBX , R "%rbx" QUADWORD)
          , (RCX , R "%rcx" QUADWORD)
          , (RDX , R "%rdx" QUADWORD)
          , (RSP , R "%rsp" QUADWORD)
          , (RBP , R "%rbp" QUADWORD)
          , (RDI , R "%rdi" QUADWORD)
          , (RSI , R "%rsi" QUADWORD)
          , (R8  , R "%r8"  QUADWORD)
          , (R9  , R "%r9"  QUADWORD)
          , (R10 , R "%r10" QUADWORD)
          , (R11 , R "%r11" QUADWORD)
          , (R12 , R "%r12" QUADWORD)
          , (R13 , R "%r13" QUADWORD)
          , (R14 , R "%r14" QUADWORD)
          , (R15 , R "%r15" QUADWORD)
          , (RIP , R "%rip" QUADWORD)
          ]
