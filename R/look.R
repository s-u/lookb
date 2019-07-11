lookb <- function(file, key, skip=0, max.lines=0)
    .Call(raw_look, as.character(file), as.character(key), as.numeric(skip), as.numeric(max.lines))
