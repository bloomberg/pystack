from pystack.colors import colored, format_colored

RANGE=100

class ColorsBenchmarkSuite:

    def setup(self):
        pass

    def time_colored(self):
        colors = ["red","green","yellow","blue","magenta","cyan","white"]
        highlights = ["on_red",
                      "on_green",
                      "on_yellow",
                      "on_blue",
                      "on_magenta",
                      "on_cyan",
                      "on_white"]
        attributes = ["bold", "dark", "underline", "blink", "reverse", "concealed"]
        for counter in range(RANGE):
            for color in colors:
                for highlight in highlights:
                    colored("Benchmark Colored",color,highlight,attributes)
        return "Successfully Benchmarks colored"
    
    def time_format_colored(self):
        colors=[
                "grey",
                "red",
                "green",
                "yellow",
                "blue",
                "magenta",
                "cyan",
                "white",
            ]
        highlights=[
                "on_grey",
                "on_red",
                "on_green",
                "on_yellow",
                "on_blue",
                "on_magenta",
                "on_cyan",
                "on_white",
            ]
        attributes=[
                "bold",
                "faint",
                "italized",
                "underline",
                "blink",
                "reverse",
                "concealed",
            ]
        for counter in range(RANGE):
            for color in colors:
                for highlight in highlights:
                    format_colored("Benchmark Format Colored",color,highlight,attributes)
        return "Successfully Benchmarks format_colored"