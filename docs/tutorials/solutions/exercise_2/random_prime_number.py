from math import sqrt
import random


'''
Check if a number is composite
Returns true if composite or False if prime
'''
def is_prime(number, itr=None):
    # Generate iterator if no value is passed
    if itr is None:
        itr = int(sqrt(number)) + 1

    # Prime number must be integers
    if type(number) != int:
        return False

    # Base case for prime numbers
    if itr == 1:
        return True

    # If the iterator evenly goes into the number, it is not prime
    if number % itr == 0:
        return False

    # Recursively check with smaller iterator
    if not is_prime(number, itr - 1):
        return False

    return True


'''
Generates a random integer 
Returns number and prime value
'''
def generate_num():
    number = random.randint(0, 100000)
    return number, is_prime(number)


def main():
    # Generate a new random number until a prime number is generated
    number, prime = generate_num()
    while not prime:
        number, prime = generate_num()

    print(f'The prime number generated is : {number}')


if __name__ == '__main__':
    main()