module FLNV.Env (Env, emptyEnv, extendEnv) where

import FLNV.Error

data Env v = Frame [(String, v)] (Env v)
           | EmptyEnvironment

emptyEnv :: Env v
emptyEnv = EmptyEnvironment

extendEnv :: [(String, v)] -> (Env v) -> (Env v)
extendEnv = Frame

addBinding :: (MonadError Error m) => String -> v -> Env v -> m (Env v)
addBinding _ _ EmptyEnvironment = throwError $
                                   InternalError "Can't add a binding to an empty environment"
addBinding key v (Frame vals parent) = case (lookup key vals) of
                                         Nothing -> return $ Frame ((key,v):vals) parent
                                         Just s  -> throwError $ DuplicateBinding key

lookupEnv :: (MonadError Error m) => String -> Env v -> m v
lookupEnv key (Frame vals parent) = case (lookup key vals) of
                                      Nothing -> lookupEnv key parent
                                      Just s  -> return s
lookupEnv key EmptyEnvironment = throwError $ Undefined key

