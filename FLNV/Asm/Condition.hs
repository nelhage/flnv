module FLNV.Asm.Condition ( Condition(..), negateCondition) where

import Data.Array

data Condition = OVERFLOW | NO_OVERFLOW
               | EQUAL    | NOT_EQUAL
               | SIGN     | NO_SIGN
               | EVEN     | ODD
               -- Unsigned integer comparisons
               | BELOW | BELOW_OR_EQUAL
               | ABOVE | ABOVE_OR_EQUAL
               -- Signed integer comparisons */
               | LESS    | LESS_OR_EQUAL
               | GREATER | GREATER_OR_EQUAL
                 deriving (Enum, Eq, Ord, Ix)

instance Show Condition where
    show = show . opsName . (conditionOps!)

negateCondition :: Condition -> Condition
negateCondition = opsNegate . (conditionOps!)


{- Bookkeeping -}

data ConditionOps = C String Condition

opsName :: ConditionOps -> String
opsName (C n _) = n

opsNegate :: ConditionOps -> Condition
opsNegate (C _ n) = n

conditionOps :: Array Condition ConditionOps
conditionOps = array (OVERFLOW, GREATER_OR_EQUAL)
               [ (OVERFLOW,         C "o"  NO_OVERFLOW)
               , (NO_OVERFLOW,      C "no" OVERFLOW)
               , (EQUAL,            C "e"  NOT_EQUAL)
               , (NOT_EQUAL,        C "ne" EQUAL)
               , (SIGN,             C "s"  NO_SIGN)
               , (NO_SIGN,          C "ns" SIGN)
               , (EVEN,             C "p"  ODD)
               , (ODD,              C "np" EVEN)
               , (BELOW,            C "b"  ABOVE_OR_EQUAL)
               , (BELOW_OR_EQUAL,   C "be" ABOVE)
               , (ABOVE,            C "a"  BELOW_OR_EQUAL)
               , (ABOVE_OR_EQUAL,   C "ae" BELOW)
               , (LESS,             C "l"  GREATER_OR_EQUAL)
               , (LESS_OR_EQUAL,    C "le" GREATER)
               , (GREATER,          C "g"  LESS_OR_EQUAL)
               , (GREATER_OR_EQUAL, C "ge" LESS)
               ]
