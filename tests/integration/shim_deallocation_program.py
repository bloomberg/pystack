import asyncio
import os


class AbortInDunderDel:
    __del__ = os.abort


async def main():
    f = asyncio.Future()
    f.set_result(AbortInDunderDel())


asyncio.run(main())
