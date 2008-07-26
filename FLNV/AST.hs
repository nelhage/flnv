module FLNV.AST (AST(..), desugar, Desugar, runDesugar) where

import FLNV.Env
import FLNV.Error
import FLNV.Expression
import Control.Monad
import Control.Monad.State
import Control.Monad.Error hiding (Error)

-- An AST is approximately an Expression, but it makes special forms
-- and calls explicit. Syntax checking occurs during the translation
-- from Expression to AST
data AST = Lambda [String] AST
         | If AST AST AST
         | Apply AST [AST]
         | AString String
         | ANumber Integer
         | ABool Bool
         | Quoted Expression
         | Sequence [AST]
         | AVar String
           deriving Show

type DesugarEnv = Env ()

newtype Desugar v = Desugar (StateT DesugarEnv (Either Error) v)
    deriving (Monad, MonadError Error, MonadState DesugarEnv)

specialForms :: [(String, Expression -> Desugar AST)]
specialForms = [ ("quote",  desugarQuoted)
               , ("lambda", desugarLambda)
               , ("if",     desugarIf)
               , ("begin",  desugarSequence)
               , ("let",    desugarLet)
               ]

runDesugar :: Desugar x -> Either Error x
runDesugar (Desugar d) = evalStateT d emptyEnv

desugar :: Expression -> Desugar AST
desugar (String s) = return $ AString s
desugar (Number n) = return $ ANumber n
desugar (Bool   b) = return $ ABool   b
desugar (Symbol v) = return $ AVar    v
desugar (Cons (Symbol form) rest) =
    do binding <- liftM (maybeLookup form) get
       case binding of
         Nothing | Just func <- lookup form specialForms -> func rest
         _  -> liftM (Apply $ AVar form) (flatten rest)
desugar (Cons what args) = do func <- desugar what
                              argl <- flatten args
                              return $ Apply func argl

desugarQuoted :: Expression -> Desugar AST
desugarQuoted (Cons thing Nil) = return $ Quoted thing
desugarQuoted form = throwError $ SyntaxError "Malformed quoted form" form

desugarLambda :: Expression -> Desugar AST
desugarLambda (Cons args body@(Cons fst rest)) =
    do argl <- (flatten args) >>= mapM assertSymbol
       modify $ extendEnv $ map (flip (,) ()) argl
       bexp <- desugarSequence body
       modify popFrame
       return $ Lambda argl bexp
desugarLambda form = throwError $ SyntaxError "Malformed Lambda" form

desugarLet :: Expression -> Desugar AST
desugarLet (Cons bindings body@(Cons fst rest)) =
    do (vars, vals) <- letBindings bindings
       lambda       <- desugarLambda (Cons (foldr Cons Nil vars) body)
       liftM (Apply lambda) $ mapM desugar vals
desugarLet err = throwError $ SyntaxError "Malformed Let" err


letBindings :: Expression -> Desugar ([Expression],[Expression])
letBindings Nil = return $ ([],[])
letBindings (Cons (Cons var (Cons val Nil)) rest) =
    do (vars,vals) <- letBindings rest
       return $ (var:vars,val:vals)
letBindings err = throwError $ SyntaxError "Malformed let bindings" err

desugarIf :: Expression -> Desugar AST
desugarIf (Cons predicate (Cons cons tail)) =
    do pAST <- desugar predicate
       cAST <- desugar cons
       aAST <- case tail of
                 Nil            -> return $ Bool False
                 (Cons ant Nil) -> return $ ant
                 err            -> throwError $ SyntaxError "Malformed alternate" err
               >>= desugar
       return $ If pAST cAST aAST

desugarSequence :: Expression -> Desugar AST
desugarSequence = (liftM Sequence) . flatten

assertSymbol :: AST -> Desugar String
assertSymbol (AVar s) = return $ s
assertSymbol other    = throwError $ SyntaxError "Expected symbol" Nil

flatten :: Expression -> Desugar [AST]
flatten Nil          = return []
flatten (Cons a b)   = do head <- desugar a
                          tail <- flatten b
                          return $ head:tail
flatten err          = throwError $ SyntaxError "Invalid sequence" err
