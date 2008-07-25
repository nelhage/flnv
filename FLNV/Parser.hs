module FLNV.Parser (readExpr) where

import FLNV.Expression
import FLNV.Error
import Control.Monad
import Text.ParserCombinators.Parsec hiding (newline)

readExpr :: (MonadError Error m) => String -> m Expression
readExpr s = case parse sExpression "scheme" s of
               Left err -> throwError $ ReadErr err
               Right val -> return val

symbolChar :: Parser Char
symbolChar = oneOf "*!$?<>=/+:_{}#" <|> letter

parseSymbol :: Parser Expression
parseSymbol = do first <- symbolChar <?> "symbol"
                 rest <- many (symbolChar <|> digit <|> char '.')
                 let symbol = first : rest
                 return $ case symbol of
                            "#t" -> Bool True
                            "#f" -> Bool False
                            _ -> Symbol symbol

parseString :: Parser Expression
parseString = do char '"' <?> "string literal"
                 str <- many (noneOf "\"")
                 char '"'
                 return $ String str

parseNumber :: Parser Expression
parseNumber = liftM (Number . read) ( many1 digit <?> "numeric literal" )

parseList :: Parser Expression
parseList = do start <- sepEndBy1 sExpression $ many space
               seed <- (char '.' >> spaces >> sExpression) <|> return Nil
               return $ foldr Cons seed start

parseQuoted :: Parser Expression
parseQuoted = do char '\''
                 val <- sExpression
                 return $ Cons (Symbol "quote") (Cons val Nil)

sExpression :: Parser Expression
sExpression = spaces >> (parseSymbol
                         <|> parseString
                         <|> parseNumber
                         <|> parseQuoted
                         <|> (try (string "()") >> return Nil)
                         <|> do char '('
                                list <- parseList
                                spaces
                                char ')'
                                return list)
