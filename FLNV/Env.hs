module FLNV.Env (Env
                , emptyEnv
                , extendEnv
                , popFrame
                , addBinding
                , lookupEnv
                , maybeLookup) where

import FLNV.Error

data Env v = Frame [(String, v)] (Env v)
           | EmptyEnvironment

emptyEnv :: Env v
emptyEnv = EmptyEnvironment

extendEnv :: [(String, v)] -> (Env v) -> (Env v)
extendEnv = Frame

popFrame :: Env v -> Env v
popFrame EmptyEnvironment = EmptyEnvironment
popFrame (Frame vals parent) = parent

addBinding :: (MonadError Error m) => String -> v -> Env v -> m (Env v)
addBinding _ _ EmptyEnvironment = throwError $
                                   InternalError "Can't add a binding to an empty environment"
addBinding key v (Frame vals parent) = case (lookup key vals) of
                                         Nothing -> return $ Frame ((key,v):vals) parent
                                         Just s  -> throwError $ DuplicateBinding key

lookupEnv :: (MonadError Error m) => String -> Env v -> m v
lookupEnv key env = case (maybeLookup key env) of
                      Nothing -> throwError $ Undefined key
                      Just s  -> return s

maybeLookup :: String -> Env v -> Maybe v
maybeLookup key (Frame vals parent) = case (lookup key vals) of
                                        Nothing -> maybeLookup key parent
                                        Just s  -> Just s
maybeLookup key EmptyEnvironment = Nothing
