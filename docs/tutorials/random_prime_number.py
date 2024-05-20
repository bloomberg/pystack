import random
from math import sqrt


def is_prime(number, *, _factor=None):
    """
    Check whether the given number is prime.

    Returns:
        True if prime, False if composite.
    """
    # If we weren't called recursively, generate the initial factor to check
    if _factor is None:
        _factor = int(sqrt(number)) + 1

    # Prime number must be integers
    if type(number) is not int:
        return False

    # If we reached 1 without finding another divisor, it's prime!
    if _factor == 1:
        return True

    # If any other factor evenly goes into the number, it is not prime
    if number % _factor == 0:
        return False

    # Recursively check with smaller factor
    return is_prime(number, _factor=_factor - 1)


def generate_num():
    """
    Generates a random integer

    Returns:
        A 2-tuple containing the randomly generated number
        and whether or not it's prime.
    """
    number = random.random()
    return number, is_prime(number)


def main():
    # Keep generating random numbers until we find a prime.
    number, prime = generate_num()
    while not prime:
        number, prime = generate_num()

    print(f"The prime number generated is: {number}")


if __name__ == "__main__":
    main()
