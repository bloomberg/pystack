Prime Number Generation
=======================

Intro
-----

Error handling is essential for any programmer. Being able to understand error messages and quickly
apply accurate fixes is vital for mental health. Unfortunately, Python's stack traces don't always
give a clear picture of everything that can go wrong...

Working Through an Example
^^^^^^^^^^^^^^^^^^^^^^^^^^

Let's go through an example where we create a function to identify prime numbers, and then generate
random numbers until we find a prime number validated through our function.

Exercise
""""""""

Take a look at the example in ``random_prime_number.py``: do you see any errors in the code?

.. literalinclude:: random_prime_number.py
   :linenos:

Take a guess, and then confirm by running the program. Does it work as you expected? If not, is it
obvious why your program is not running properly?

Expectations vs Reality
"""""""""""""""""""""""

Often an exception is thrown and the stack trace Python prints allows a quick solution to become
apparent, but sometimes we get no feedback at all. In this case, we accidentally entered an infinite
loop! But where in the program did we go wrong? Does the logic in the function ``is_prime`` work
correctly, or is the error somewhere else?

Since the code enters an infinite loop, Python never gives us a stack trace. This can lead to a lot
of confusion and no information as to where the error occurred. We're here to see if there is
a better way to debug your infinite loop.

Challenge
"""""""""

Use ``pystack`` on the running program and see if you can identify the bug. Be sure to check what
options may help you locate the errors in your code.

.. raw:: html

   <details>
   <summary><i>Hint 1</i></summary>

Check your local variables using ``pystack``.

.. raw:: html

   </details>

.. raw:: html

   <details>
   <summary><i>Hint 2</i></summary>

Try the following command while your program is running: ``pystack remote --locals {PID}``.

.. raw:: html

   </details>

Solutions
"""""""""

.. raw:: html

   <details>
   <summary><i>Toggle to see the solution</i></summary>

After using PyStack and printing the locals you can see that we accidentally used the wrong random
function! In our current implementation, ``random.random()`` only returns floats between [0.0, 1.0).
Our ``is_prime`` function has correct logic, but the function will never get passed a prime number,
because of the random number generator we chose!

.. literalinclude:: random_prime_number.py
   :linenos:
   :pyobject: generate_num
   :emphasize-lines: 9

A quick fix is to replace the ``random.random()`` call with ``random.randint(0, 100000)``. With this
quick fix, our program will now generate random prime numbers!

.. raw:: html

   </details>

Conclusion
^^^^^^^^^^

Sometimes, finding the bug in your code is not as simple as looking at a stack trace. Infinite loops
are common logical errors in programs, but can be difficult to find. Instead of spending excessive
time searching through lots of code, using PyStack can help you find these logical errors.
