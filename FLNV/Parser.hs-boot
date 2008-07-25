module FLNV.Parser (Expression(..)) where

data Expression = Symbol String
                | Number Integer
                | String String
                | Bool Bool
                | Cons Expression Expression
                | Nil
                  deriving (Eq)

instance Show Expression
