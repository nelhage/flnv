module FLNV.Expression (Expression(..)) where

data Expression = Symbol String
                | Number Integer
                | String String
                | Bool Bool
                | Cons Expression Expression
                | Nil
                  deriving (Eq)

instance Show Expression where
    show (Symbol s) = s
    show (Number n) = show n
    show (String s) = '"' : s ++ "\""
    show (Bool True) = "#t"
    show (Bool False) = "#f"
    show c@(Cons a b) = "(" ++ showListInner c ++ ")"
    show Nil = "()"

showListInner :: Expression -> String
showListInner (Cons a b@(Cons _ _)) = show a ++ " " ++ showListInner b
showListInner (Cons a Nil) = show a
showListInner (Cons a b) = show a ++ " . " ++ showListInner b
showListInner Nil = ""
showListInner x = show x

