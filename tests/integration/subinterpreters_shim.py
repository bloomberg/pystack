try:
    from concurrent import interpreters  # type: ignore

    def run_in_new_interpreter(code):
        interpreters.create().exec(code)

except ImportError:
    try:
        import _interpreters  # type: ignore

        def run_in_new_interpreter(code):
            _interpreters.exec(_interpreters.create(), code)

    except ImportError:
        import _xxsubinterpreters  # type: ignore

        def run_in_new_interpreter(code):
            _xxsubinterpreters.run_string(
                _xxsubinterpreters.create(isolated=False), code
            )
