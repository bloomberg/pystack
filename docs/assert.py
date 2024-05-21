import ctypes


def assertion():

    number = 42.0

    try:
        assert number > 0, "Number must be positive!"
        assert number == 42.0, "Number is not the right size!"
        assert number < 100, "Number is too large!"
        assert type(number) is int, "Number must be an integer!"

    except AssertionError as e:
        print("Oops: ", e)

        # Load the C standard library
        libc = ctypes.CDLL("libc.so.6")

        # Call the abort function
        libc.abort()

    else:
        print("Number passed all asserts!")


if __name__ == "__main__":
    assertion()
