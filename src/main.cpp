#include <algorithm>
#include <ctype.h>
#include <functional>
#include <iterator>
#include <numeric>
#include <string>
#include <vcruntime_string.h>
#include <vector>

#include <cxxopts.hpp>
#include <fmt/format.h>

#include "libbmp.h"

enum class Command { kUnkown = 0, kHistogram, kEqualization, kMedianGreyLevel };

struct RGBHistogram {
  int red[256];
  int blue[256];
  int green[256];
};

struct Rectangle {
  int x, y;
  int width, height;
};

struct RGBColor {
  using byte = unsigned char;
  byte r, g, b;
};

namespace {

void ClearImage(BmpImg &img) {
  int height = img.get_height();
  int width = img.get_width();

  for (int x = 0; x < width; x++) {
    for (int y = 0; y < height; y++) {
      img.set_pixel(x, y, 0, 0, 0);
    }
  }
}

void FillRectangle(BmpImg &img, const Rectangle &rect, const RGBColor &color) {
  for (int x = rect.x; x < rect.x + rect.width; x++) {
    for (int y = rect.y; y < rect.y + rect.height; y++) {
      img.set_pixel(x, y, color.r, color.g, color.b);
    }
  }
}

void DrawRectangle(BmpImg &img, const Rectangle &rect, const RGBColor &color) {
  for (int x = rect.x; x < rect.x + rect.width; x++) {
    for (int y = rect.y; y < rect.y + rect.height; y++) {
      bool should_draw = x == rect.x || y == rect.y ||
                         x == (rect.x + rect.width - 1) ||
                         y == (rect.y + rect.height - 1);
      if (should_draw) {
        img.set_pixel(x, y, color.r, color.g, color.b);
      }
    }
  }
}

void DrawLine(BmpImg &img, int x, int y, int xf, int yf,
              const RGBColor &color) {
  for (int i = x; i <= xf; i++) {
    for (int j = y; j <= yf; j++) {
      img.set_pixel(i, j, color.r, color.g, color.b);
    }
  }
}

} // namespace

Command CommandByMethod(const std::string &command) {
  if (command == "histogram") {
    return Command::kHistogram;
  }

  if (command == "equalize") {
    return Command::kEqualization;
  }

  if (command == "median_grey_level") {
    return Command::kMedianGreyLevel;
  }

  return Command::kUnkown;
}

RGBHistogram GetHistogram(BmpImg &img) {
  RGBHistogram histogram{};
  memset(&histogram, 0, sizeof(RGBHistogram));

  int height = img.get_height();
  int width = img.get_width();

  for (int x = 0; x < width; x++) {
    for (int y = 0; y < height; y++) {
      histogram.red[img.red_at(x, y)]++;
      histogram.green[img.green_at(x, y)]++;
      histogram.blue[img.blue_at(x, y)]++;
    }
  }

  return histogram;
}

BmpImg CreateHistogramImage(RGBHistogram &histogram) {
  const int lr_borders = 30;
  const int tb_borders = 10;
  const int in_between_borders = 30;

  const int graph_width = 256;
  const int graph_height = 256;

  const RGBColor white = {.r = 255, .g = 255, .b = 255};
  const RGBColor red = {.r = 255, .g = 0, .b = 0};
  const RGBColor green = {.r = 0, .g = 255, .b = 0};
  const RGBColor blue = {.r = 0, .g = 0, .b = 255};

  const Rectangle red_rect = {.x = lr_borders,
                              .y = tb_borders,
                              .width = graph_width,
                              .height = graph_height};

  const Rectangle green_rect = {.x = lr_borders,
                                .y = tb_borders + graph_height +
                                     in_between_borders,
                                .width = graph_width,
                                .height = graph_height};

  const Rectangle blue_rect = {.x = lr_borders,
                               .y = tb_borders +
                                    2 * (graph_height + in_between_borders),
                               .width = graph_width,
                               .height = graph_height};
  int width = 2 * lr_borders + graph_width;
  int height = 2 * tb_borders + 2 * in_between_borders + 3 * graph_height;

  BmpImg graph(width, height);

  ClearImage(graph);

  DrawRectangle(graph, red_rect, white);
  DrawRectangle(graph, green_rect, white);
  DrawRectangle(graph, blue_rect, white);

  int max_red = *std::max_element(histogram.red, histogram.red + 256);
  int max_green = *std::max_element(histogram.green, histogram.green + 256);
  int max_blue = *std::max_element(histogram.blue, histogram.blue + 256);

  for (int i = 0; i < 256; i++) {
    DrawLine(graph, red_rect.x + i, red_rect.y, red_rect.x + i,
             red_rect.y + graph_height * (histogram.red[i] / (1.0 * max_red)),
             red);
  }
  for (int i = 0; i < 256; i++) {
    DrawLine(graph, green_rect.x + i, green_rect.y, green_rect.x + i,
             green_rect.y +
                 graph_height * (histogram.green[i] / (1.0 * max_green)),
             green);
  }
  for (int i = 0; i < 256; i++) {
    DrawLine(graph, blue_rect.x + i, blue_rect.y, blue_rect.x + i,
             blue_rect.y +
                 graph_height * (histogram.blue[i] / (1.0 * max_blue)),
             blue);
  }

  return graph;
}

BmpImg Equalize(BmpImg &img) {
  using byte = unsigned char;
  RGBHistogram histogram = GetHistogram(img);

  int height = img.get_height();
  int width = img.get_width();
  int total_pixels = width * height;
  int red_cdf[256];
  int green_cdf[256];
  int blue_cdf[256];

  memset(red_cdf, 0, sizeof(red_cdf));
  memset(green_cdf, 0, sizeof(green_cdf));
  memset(blue_cdf, 0, sizeof(blue_cdf));

  auto set_cdf = [](int size, int *input, int *output) -> void {
    output[0] = input[0];

    for (int i = 1; i < size; i++) {
      output[i] = input[i] + output[i - 1];
    }
  };

  auto get_min_on_cdf = [](int *cdf, int size) -> int {
    for (int i = 0; i < size; i++) {
      if (cdf[i]) {
        return cdf[i];
      }
    }
    return 0;
  };

  auto equalized_value = [](int *cdf, int value, int min_value,
                            int total) -> byte {
    return static_cast<int>(
        255 * ((cdf[value] - min_value) / (1.0 * (total - min_value))));
  };

  set_cdf(256, histogram.red, red_cdf);
  set_cdf(256, histogram.green, green_cdf);
  set_cdf(256, histogram.blue, blue_cdf);

  int min_red_cdf = get_min_on_cdf(red_cdf, 256);
  int min_green_cdf = get_min_on_cdf(green_cdf, 256);
  int min_blue_cdf = get_min_on_cdf(blue_cdf, 256);

  BmpImg output_img(width, height);

  for (int x = 0; x < width; x++) {
    for (int y = 0; y < height; y++) {
      byte red_value = img.red_at(x, y);
      byte green_value = img.green_at(x, y);
      byte blue_value = img.blue_at(x, y);

      byte equalized_red =
          equalized_value(red_cdf, red_value, min_red_cdf, total_pixels);
      byte equalized_green =
          equalized_value(green_cdf, green_value, min_green_cdf, total_pixels);
      byte equalized_blue =
          equalized_value(blue_cdf, blue_value, min_blue_cdf, total_pixels);

      output_img.set_pixel(x, y, equalized_red, equalized_green,
                           equalized_blue);
    }
  }

  return output_img;
}

BmpImg MedianGreyLevel(BmpImg &img) {
  using byte = unsigned char;
  int width = img.get_width();
  int height = img.get_height();

  RGBHistogram histogram = GetHistogram(img);

  auto non_zero = [](int value) -> bool { return value != 0; };

  RGBColor min_intensity{
      .r = static_cast<byte>(std::distance(
          histogram.red,
          std::find_if(histogram.red, histogram.red + 256, non_zero))),
      .g = static_cast<byte>(std::distance(
          histogram.green,
          std::find_if(histogram.green, histogram.green + 256, non_zero))),
      .b = static_cast<byte>(std::distance(
          histogram.blue,
          std::find_if(histogram.blue, histogram.blue + 256, non_zero)))};

  RGBColor max_intensity{
      .r = static_cast<byte>(std::distance(
          histogram.red, std::max_element(histogram.red, histogram.red + 256))),
      .g = static_cast<byte>(std::distance(
          histogram.green,
          std::max_element(histogram.green, histogram.green + 256))),
      .b = static_cast<byte>(std::distance(
          histogram.blue,
          std::max_element(histogram.blue, histogram.blue + 256)))};

  RGBColor median_cut{
      .r = static_cast<byte>((max_intensity.r - min_intensity.r) >> 1),
      .g = static_cast<byte>((max_intensity.g - min_intensity.g) >> 1),
      .b = static_cast<byte>((max_intensity.b - min_intensity.b) >> 1)};

  BmpImg output_image(width, height);

  for (int x = 0; x < width; x++) {
    for (int y = 0; y < height; y++) {
      RGBColor value{.r = img.red_at(x, y),
                     .g = img.green_at(x, y),
                     .b = img.blue_at(x, y)};
      RGBColor grey_value{};

      grey_value.r = (value.r < median_cut.r) ? 0 : 255;
      grey_value.g = (value.g < median_cut.g) ? 0 : 255;
      grey_value.b = (value.b < median_cut.b) ? 0 : 255;

      output_image.set_pixel(x, y, grey_value.r, grey_value.g, grey_value.b);
    }
  }

  return output_image;
}

int main(int argc, char **argv) {
  cxxopts::Options options("PDI_LI", "Process some method of image processing");
  options.add_options()("i,input", "The input bmp",
                        cxxopts::value<std::string>());
  options.add_options()("m,method", "The processing method",
                        cxxopts::value<std::string>());
  options.add_options()("o,output", "The output bmp",
                        cxxopts::value<std::string>());

  auto result = options.parse(argc, argv);
  std::string input_bmp = result["input"].as<std::string>();
  std::string method = result["method"].as<std::string>();
  std::string output_bmp = result["output"].as<std::string>();

  fmt::print("Using args: {} {} {}\n", input_bmp, method, output_bmp);

  BmpImg input_image;

  input_image.read(input_bmp);

  switch (CommandByMethod(method)) {
    using enum Command;

  case kUnkown: {
    fmt::print("Unkown command\n");
    return 1;
  } break;

  case kHistogram: {

    RGBHistogram histogram = GetHistogram(input_image);

    BmpImg histogram_image = CreateHistogramImage(histogram);

    histogram_image.write(output_bmp);
  } break;

  case kEqualization: {
    BmpImg output_image = Equalize(input_image);

    output_image.write(output_bmp);
  } break;

  case kMedianGreyLevel: {
    BmpImg output_image = MedianGreyLevel(input_image);

    output_image.write(output_bmp);
  } break;

  default:
    break;
  }

  return 0;
}