#!/bin/bash

[[ -z "$1" ]] && echo "$0 <version>" && exit 1

VERSION=$1
GIT_USER=$(git config --get user.name)
GIT_EMAIL=$(git config --get user.email)
TIMESTAMP=$(date -Ru)

CHANGELOG=$(cat << end-of-doc
python-pystack (${VERSION}) unstable; urgency=low

  * Package pystack ${VERSION} release.

 -- ${GIT_USER} <${GIT_EMAIL}>  ${TIMESTAMP}


end-of-doc
)

printf '%s\n\n%s\n' "${CHANGELOG}" "$(cat debian/changelog)" > debian/changelog
