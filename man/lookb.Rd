\name{lookb}
\alias{lookb}
\title{
Prefix Binary Search in a Text File
}
\description{
\code{lookb} performs a binary search of a prefix in a text file. The
result is a raw vector of the matching lines.
}
\usage{
lookb(file, key, skip = 0, max.lines = 0)
}
\arguments{
  \item{file}{path to the sorted file}
  \item{key}{string, prefix to look for}
  \item{skip}{numeric, number of lines to skip}
  \item{max.lines}{numeric, if greater than zero then the result set
    will be truncated to the specified number of lines at most}
}
\details{
  \code{lookb} uses \code{mmap()} to map the file into virtual memory
  (hence it must be an acutal file) and then performs binary search to
  find the section of the file with the matching prefix. Note that all
  operations are prefromed as bytes, the file is assumed to be sorted in
  strictly binary order, e.g., via \code{LANG=C sort}.
}
\value{
  Raw vector with the resulting lines, it will have the length 0 if no
  match was found.
}
\author{
Simon Urbanek
}
\note{
  The name is derived from the \code{look -b} unix command.
}
\examples{
out <- tempfile()

# create a sorted file
system(paste("LANG=C sort < ",
       shQuote(file.path(R.home("doc"), "AUTHORS")),
       " > ", shQuote(out)))

# look for "Mart" prefix
cat(rawToChar(lookb::lookb(out, "Mart")))

unlink(out)
}
\keyword{manip}
