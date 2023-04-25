PYTHON ?= python
DOCKER_IMAGE ?= pystack
DOCKER_SRC_DIR ?= /src

# Doc generation variables
UPSTREAM_GIT_REMOTE ?= origin
DOCSBUILDDIR := docs/_build
HTMLDIR := $(DOCSBUILDDIR)/html
PIP_INSTALL=$(PYTHON) -m pip install

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
	$(ENV) CYTHON_TEST_MACROS=1 $(PIP_INSTALL) -e . -r requirements-test.txt

.PHONY: docker-build
docker-build:  ## Build the Docker image
	docker build -t $(DOCKER_IMAGE) .

.PHONY: docker-rm
docker-rm:  ## Remove the Docker image
	docker kill pystack || true
	docker rmi $(DOCKER_IMAGE)

.PHONY: docker-shell
docker-shell: docker-build ## Run a shell in the Docker image
	## If container exists, run bash in it
	@if docker ps -a | grep -q pystack; then \
		docker start pystack && docker exec -it pystack /bin/bash; \
	fi
	## Run the container
	@docker run -it --name pystack --rm \
		--privileged \
		--rm \
		-v $(PWD):$(DOCKER_SRC_DIR) \
		-w $(DOCKER_SRC_DIR) \
		$(DOCKER_IMAGE) \
		/bin/bash

.PHONY: check
check:  ## Run the test suite
	$(PYTHON) -m pytest -vvv --log-cli-level=info -s --color=yes $(PYTEST_ARGS) tests

.PHONY: pycoverage
pycoverage:  ## Run the test suite, with Python code coverage
	$(PYTHON) -m pytest -vvv --log-cli-level=info -s --color=yes \
				--cov=pystack --cov=tests --cov-config=pyproject.toml --cov-report=term \
				--cov-append $(PYTEST_ARGS) tests --cov-fail-under=92
	$(PYTHON) -m coverage lcov -i -o pycoverage.lcov
	genhtml *coverage.lcov --branch-coverage --output-directory pystack-coverage

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
	lcov --capture --directory .  --output-file cppcoverage.lcov
	lcov --extract cppcoverage.lcov '*/src/pystack/_pystack/*' --output-file cppcoverage.lcov
	genhtml *coverage.lcov --branch-coverage --output-directory pystack-coverage

.PHONY: format
format:  ## Autoformat all files
	$(PYTHON) -m pre_commit run --all-files

.PHONY: lint
lint:  ## Lint all files
	$(PYTHON) -m pre_commit run --all-files
	$(PYTHON) -m mypy src/pystack --strict --ignore-missing-imports
	$(PYTHON) -m mypy tests --ignore-missing-imports

.PHONY: docs
docs:  ## Generate documentation
	$(MAKE) -C docs clean
	$(MAKE) -C docs html

.PHONY: gh-pages
gh-pages:  ## Publish documentation on GitHub Pages
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
	find . | grep -E '(\.o|\.gcda|\.gcno|\.gcov\.json\.gz)' | xargs rm -rf
	find . | grep -E '(__pycache__|\.pyc|\.pyo)' | xargs rm -rf
	rm -rf build
	rm -f src/pystack/_pystack.*.so
	rm -f {cpp,py}coverage.lcov
	rm -rf pystack-coverage


.PHONY: check_release_env
check_release_env:
ifndef RELEASE
	$(error RELEASE is undefined. Please set it to either ["major", "minor", "patch"])
endif

.PHONY: bump_version
bump_version: check_release_env
	bump2version $(RELEASE)
		$(eval NEW_VERSION := $(shell bump2version \
	                            --allow-dirty \
	                            --dry-run \
	                            --list $(RELEASE) \
	                            | tail -1 \
	                            | sed s,"^.*=",,))
	git commit --amend --no-edit

.PHONY: gen_news
gen_news: check_release_env
	$(eval CURRENT_VERSION := $(shell bump2version \
	                            --allow-dirty \
	                            --dry-run \
	                            --list $(RELEASE) \
	                            | grep current_version \
	                            | sed s,"^.*=",,))
	$(PYEXEC) towncrier build --version $(CURRENT_VERSION) --name pystack

.PHONY: release
release: check_release_env bump_version gen_news  ## Prepare release

.PHONY: help
help:  ## Print this message
	@grep -E '^[a-zA-Z_-]+:.*?## .*$$' $(MAKEFILE_LIST) | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[36m%-30s\033[0m %s\n", $$1, $$2}'
