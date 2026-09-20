from conan import ConanFile
from conan.tools.cmake import cmake_layout

class LearnAVRecipe(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps", "CMakeToolchain"

    requires = (
        "ffmpeg/4.4.4",
        "stb/cci.20240531",
        "glm/cci.20230113",
    )

    def layout(self):
        cmake_layout(self)

    def configure(self):
        self.options["ffmpeg"].with_libiconv = False
        self.options["ffmpeg"].with_libx264 = False
        self.options["ffmpeg"].with_libx265 = False
        self.options["ffmpeg"].with_libfdk_aac = False
        self.options["ffmpeg"].with_ssl = False
        self.options["ffmpeg"].with_libaom = False
        self.options["ffmpeg"].with_libdav1d = False
        self.options["ffmpeg"].with_libsvtav1 = False
