PYTHON ?= python
CLANG_FORMAT ?= clang-format
PRETTIER ?= prettier --no-editorconfig

# Doc generation variables
UPSTREAM_GIT_REMOTE ?= origin
DOCSBUILDDIR := docs/_build
HTMLDIR := $(DOCSBUILDDIR)/html
PKG_CONFIG_PATH ?= /opt/bb/lib64/pkgconfig
PIP_INSTALL=PKG_CONFIG_PATH="$(PKG_CONFIG_PATH)" $(PYTHON) -m pip install

markdown_files := '**/*.md'
cpp_files := $(shell find src/pystack/_pystack -name \*.cpp -o -name \*.h)
python_files := $(shell find src tests -name \*.py -not -path '*/\.*')
mypy_files := $(shell find src tests -name \*.pyi -not -path '*/\.*')
cython_files := $(shell find src tests -name \*.pyx -or -name \*.pxd -not -path '*/\.*')

# Use this to inject arbitrary commands before the make targets (e.g. docker)
ENV :=

.PHONY: build
build:  ## (default) Build package extensions in-place
	$(PYTHON) setup.py build_ext --inplace

.PHONY: dist
dist:  ## Generate Python distribution files
	$(PYTHON) -m pep517.build .

.PHONY: install-sdist
install-sdist: dist  ## Install from source distribution
	$(ENV) $(PIP_INSTALL) $(wildcard dist/*.tar.gz)

.PHONY: test-install
test-install:  ## Install with test dependencies
	$(ENV) CYTHON_TEST_MACROS=1 $(PIP_INSTALL) -e .[test]

.PHONY: check
check:  ## Run the test suite
	$(PYTHON) -m pytest -vvv --log-cli-level=info -s --color=yes $(PYTEST_ARGS) tests

.PHONY: pycoverage
pycoverage:  ## Run the test suite, with Python code coverage
	$(PYTHON) -m pytest -vvv --log-cli-level=info -s --color=yes \
				--cov=pystack --cov-config=tox.ini --cov-report=term \
				--cov-append $(PYTEST_ARGS) tests --cov-fail-under=97

.PHONY: valgrind
valgrind:  ## Run valgrind, with the correct configuration
	PYTHONMALLOC=malloc valgrind --suppressions=./valgrind.supp --leak-check=full --show-leak-kinds=definite \
	--error-exitcode=1 $(PYTHON) -m pytest tests/integration/test_smoke.py -v

.PHONY: ccoverage
ccoverage:  ## Run the test suite, with C++ code coverage
	$(MAKE) clean
	CFLAGS="$(CFLAGS) -O0 -pg --coverage" $(MAKE) build
	$(MAKE) check
	gcov -i build/*/src/pystack/_pystack -i -d
	lcov --capture --directory .  --output-file pystack.info
	lcov --extract pystack.info '*/src/pystack/*' --output-file pystack.info
	genhtml pystack.info --output-directory pystack-coverage
	find . | grep -E '(\.gcda|\.gcno|\.gcov\.json\.gz)' | xargs rm -rf

.PHONY: format-markdown
format-markdown:  ## Autoformat markdown files
	$(PRETTIER) --write $(markdown_files)

.PHONY: format
format: format-markdown  ## Autoformat all files
	$(PYTHON) -m isort $(python_files) $(cython_files)
	$(PYTHON) -m black $(python_files) $(mypy_files)
	$(CLANG_FORMAT) -i $(cpp_files)

.PHONY: lint-markdown
lint-markdown:  ## Lint markdown files
	$(PRETTIER) --check $(markdown_files)

.PHONY: lint
lint: lint-markdown  ## Lint all files
	$(PYTHON) -m isort --check $(python_files) $(cython_files)
	$(PYTHON) -m flake8 $(python_files)
	$(PYTHON) -m black --check $(python_files) $(mypy_files)
	$(PYTHON) -m mypy src/pystack --strict --ignore-missing-imports
	$(CLANG_FORMAT) --Werror --dry-run $(cpp_files)

.PHONY: docs
docs:  ## Generate documentation
	$(MAKE) -C docs clean
	$(MAKE) -C docs html

.PHONY: gh-pages
gh-pages:  ## Publish documentation on BBGitHub Pages
	$(eval GIT_REMOTE := $(shell git remote get-url $(UPSTREAM_GIT_REMOTE)))
	$(eval COMMIT_HASH := $(shell git rev-parse HEAD))
	touch $(HTMLDIR)/.nojekyll
	@echo -n "Documentation ready, push to $(GIT_REMOTE)? [Y/n] " && read ans && [ $${ans:-Y} == Y ]
	git init $(HTMLDIR)
	GIT_DIR=$(HTMLDIR)/.git GIT_WORK_TREE=$(HTMLDIR) git add -A
	GIT_DIR=$(HTMLDIR)/.git git commit -m "Documentation for commit $(COMMIT_HASH)"
	GIT_DIR=$(HTMLDIR)/.git git push $(GIT_REMOTE) HEAD:gh-pages --force
	rm -rf $(HTMLDIR)/.git


.PHONY: clean
clean:  ## Clean any built/generated artifacts
	find . | grep -E '(\.o|\.so|\.gcda|\.gcno|\.gcov\.json\.gz)' | xargs rm -rf
	find . | grep -E '(__pycache__|\.pyc|\.pyo)' | xargs rm -rf
	rm -f pystack.info
	rm -rf pystack-coverage

.PHONY: bump_version
bump_version:
	bump2version $(RELEASE)
		$(eval NEW_VERSION := $(shell bump2version \
	                            --allow-dirty \
	                            --dry-run \
	                            --list $(RELEASE) \
	                            | tail -1 \
	                            | sed s,"^.*=",,))
	git commit --amend --no-edit

.PHONY: gen_news
gen_news:
	$(eval CURRENT_VERSION := $(shell bump2version \
	                            --allow-dirty \
	                            --dry-run \
	                            --list $(RELEASE) \
	                            | grep current_version \
	                            | sed s,"^.*=",,))
	$(PYEXEC) towncrier build --version $(CURRENT_VERSION) --name pystack

.PHONY: release
release: bump_version gen_news  ## Prepare release

.PHONY: help
help:  ## Print this message
	@grep -E '^[a-zA-Z_-]+:.*?## .*$$' $(MAKEFILE_LIST) | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[36m%-30s\033[0m %s\n", $$1, $$2}'
