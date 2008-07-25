module FLNV.Error (Error(..), throwError, MonadError) where

import Text.ParserCombinators.Parsec (ParseError)

import Control.Monad.Error (throwError, MonadError)
import qualified Control.Monad.Error as E

import {-# SOURCE #-} FLNV.Parser (Expression)

data Error = Error String
           | ReadErr ParseError
           | SyntaxError String Expression
             deriving Show

instance E.Error Error where
    noMsg = Error "An error has occurred"
    strMsg = Error

