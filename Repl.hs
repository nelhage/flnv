import FLNV.Expression
import FLNV.Parser
import FLNV.AST
import FLNV.Error
import System.IO
import System.Exit

runParser :: String -> IO Expression
runParser str = case (readExpr str :: Either Error Expression) of
                  Right expr -> return expr
                  Left err -> do putStrLn (show err)
                                 exitWith $ ExitFailure 1

doDesugar :: Expression -> IO AST
doDesugar exp = do case (runDesugar $ desugar exp) of
                     (Left err) -> do putStrLn (show err)
                                      exitWith $ ExitFailure 2
                     (Right ast) -> return ast

main :: IO ()
main = do prog <- getContents
          expr <- runParser prog
          ast  <- doDesugar expr
          putStrLn $ show ast
          exitWith ExitSuccess
