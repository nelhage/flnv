module FLNV.AST where

import FLNV.Error
import FLNV.Expression
import Control.Monad

-- An AST is approximately an Expression, but it makes special forms
-- and calls explicit. Syntax checking occurs during the translation
-- from Expression to AST
data AST = Define String Expression
         | Lambda [String] AST
         | If AST AST AST
         | Apply AST [AST]
         | AString String
         | ANumber Integer
         | ABool Bool
         | Quoted Expression
         | Sequence [AST]
         | Variable String

specialForms :: (MonadError Error m) => [(String, Expression -> m AST)]
specialForms = [ ("quote",  desugarQuoted)
               , ("lambda", desugarLambda)
               , ("if",     desugarIf)
               , ("begin",  desugarSequence)]

desugar :: (MonadError Error m) => Expression -> m AST
desugar (String s) = return $ AString  s
desugar (Number n) = return $ ANumber  n
desugar (Bool   b) = return $ ABool    b
desugar (Symbol v) = return $ Variable v
desugar (Cons (Symbol form) rest)
    | Just func <- lookup form specialForms = func rest
desugar (Cons what args) = do func <- desugar what
                              argl <- flatten args
                              return $ Apply func argl

desugarQuoted :: (MonadError Error m) => Expression -> m AST
desugarQuoted (Cons thing Nil) = return $ Quoted thing
desugarQuoted form = throwError $ SyntaxError "Malformed quoted form" form

desugarLambda :: (MonadError Error m) => Expression -> m AST
desugarLambda (Cons args body@(Cons fst rest)) =
    do argl <- (flatten args) >>= mapM assertSymbol
       bexp <- desugarSequence body
       return $ Lambda argl bexp
desugarLambda form = throwError $ SyntaxError "Malformed Lambda" form

desugarIf :: (MonadError Error m) => Expression -> m AST
desugarIf (Cons predicate (Cons cons tail)) =
    do pAST <- desugar predicate
       cAST <- desugar cons
       aAST <- case tail of
                 Nil            -> return $ Bool False
                 (Cons ant Nil) -> return $ ant
                 err            -> throwError $ SyntaxError "Malformed alternate" err
               >>= desugar
       return $ If pAST cAST aAST

desugarSequence :: (MonadError Error m) => Expression -> m AST
desugarSequence = (liftM Sequence) . flatten

assertSymbol :: (MonadError Error m) => AST -> m String
assertSymbol (Variable s) = return $ s
assertSymbol other = throwError $ SyntaxError "Expected symbol" Nil

flatten :: (MonadError Error m) => Expression -> m [AST]
flatten Nil          = return []
flatten (Cons a b)   = do head <- desugar a
                          tail <- flatten b
                          return $ head:tail
flatten v            = throwError $ SyntaxError "Invalid sequence" v
