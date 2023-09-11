/// Copyright: Made by Marcos Oliveira (mhco@cin.ufpe.br 2023)
/// Github: mhco0

#include <algorithm>
#include <ctype.h>
#include <functional>
#include <iterator>
#include <numeric>
#include <stdint.h>
#include <string>
#include <vcruntime_string.h>
#include <vector>

#include <cxxopts.hpp>
#include <fmt/format.h>

#include "libbmp.h"

using byte = unsigned char;

enum class Command {
  kUnkown = 0,
  kHistogram,
  kEqualization,
  kCutout,
  kTwoPeaks
};

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

/// @brief Parses the @p command string of the user in some @see Command
/// enumaration
/// @param command The command provide by the user
/// @return Some @see Command enumeration
Command CommandByMethod(const std::string &command) {
  if (command == "histogram") {
    return Command::kHistogram;
  }

  if (command == "equalize") {
    return Command::kEqualization;
  }

  if (command == "cutout") {
    return Command::kCutout;
  }

  if (command == "two_peaks") {
    return Command::kTwoPeaks;
  }

  return Command::kUnkown;
}

/// @brief Retrieves the histogram of some bitmap @p img
/// @param img The image to retrieve the histogram
/// @return A @see RGBHistogram struct with the histogram of the red, green and
/// blue channel of the image.
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

/// @brief Creates a interpolated image with the visual information about the
/// histogram of some image
/// @param histogram The histogram information about each channel of the image
/// @return A bitmap image with the histogram drawed upside down.
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

/// @brief Converts some bitmap @p img in only true black and white value on
/// each matrix.
/// @param img [in | out] The image to be binarized.
/// @param red_cut_point The point of cut for the red channel.
/// @param green_cut_point The point of cut for the green channel.
/// @param blue_cut_point The point of cut for the blue channel.
void Binarize(BmpImg &img, byte red_cut_point, byte green_cut_point,
              byte blue_cut_point) {
  int width = img.get_width();
  int height = img.get_height();

  for (int x = 0; x < width; ++x) {
    for (int y = 0; y < height; ++y) {
      RGBColor value{.r = img.red_at(x, y),
                     .g = img.green_at(x, y),
                     .b = img.blue_at(x, y)};
      RGBColor grey_value{};

      grey_value.r = static_cast<byte>((value.r < red_cut_point) ? 0 : 255);
      grey_value.g = static_cast<byte>((value.g < green_cut_point) ? 0 : 255);
      grey_value.b = static_cast<byte>((value.b < blue_cut_point) ? 0 : 255);

      img.set_pixel(x, y, grey_value.r, grey_value.g, grey_value.b);
    }
  }
}

/// @brief Applys the equalization algorithm on the @p img .
/// @param img The image to have the histogram equalizated.
void Equalize(BmpImg &img) {
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

  for (int x = 0; x < width; ++x) {
    for (int y = 0; y < height; ++y) {
      byte red_value = img.red_at(x, y);
      byte green_value = img.green_at(x, y);
      byte blue_value = img.blue_at(x, y);

      byte equalized_red =
          equalized_value(red_cdf, red_value, min_red_cdf, total_pixels);
      byte equalized_green =
          equalized_value(green_cdf, green_value, min_green_cdf, total_pixels);
      byte equalized_blue =
          equalized_value(blue_cdf, blue_value, min_blue_cdf, total_pixels);

      img.set_pixel(x, y, equalized_red, equalized_green, equalized_blue);
    }
  }
}

/// @brief Applies the cutout algorithm on the @p img .
/// @param img [in | out] The image to be binarized.
void Cutout(BmpImg &img) { Binarize(img, 128, 128, 128); }

/// @brief Applies the Two Peaks algorithm on the @p img .
/// @param img [in | out]The image to be binarized.
void TwoPeaks(BmpImg &img) {
  RGBHistogram histogram = GetHistogram(img);

  auto get_index_of_max_value = [](auto *input, int size) -> byte {
    return static_cast<byte>(
        std::distance(input, std::max_element(input, input + size)));
  };

  byte first_peaks[3] = {get_index_of_max_value(histogram.red, 256),
                         get_index_of_max_value(histogram.green, 256),
                         get_index_of_max_value(histogram.blue, 256)};

  int64_t red_sparse_distances[256];
  int64_t green_sparse_distances[256];
  int64_t blue_sparse_distances[256];

  memset(red_sparse_distances, 0, sizeof(red_sparse_distances));
  memset(green_sparse_distances, 0, sizeof(green_sparse_distances));
  memset(blue_sparse_distances, 0, sizeof(blue_sparse_distances));

  for (int i = 0; i < 256; i++) {
    red_sparse_distances[i] =
        (i - first_peaks[0]) * (i - first_peaks[0]) * histogram.red[i];
    green_sparse_distances[i] =
        (i - first_peaks[1]) * (i - first_peaks[1]) * histogram.green[i];
    blue_sparse_distances[i] =
        (i - first_peaks[2]) * (i - first_peaks[2]) * histogram.blue[i];
  }

  byte second_peaks[3] = {get_index_of_max_value(red_sparse_distances, 256),
                          get_index_of_max_value(green_sparse_distances, 256),
                          get_index_of_max_value(blue_sparse_distances, 256)};
  byte cut_points[3] = {0, 0, 0};
  for (int i = 0; i < 3; i++) {
    cut_points[i] = (first_peaks[i] + second_peaks[i]) >> 1;
  }

  Binarize(img, cut_points[0], cut_points[1], cut_points[2]);
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
    Equalize(input_image);

    input_image.write(output_bmp);
  } break;

  case kCutout: {
    Cutout(input_image);

    input_image.write(output_bmp);
  } break;

  case kTwoPeaks: {
    TwoPeaks(input_image);

    input_image.write(output_bmp);
  } break;

  default:
    break;
  }

  return 0;
}