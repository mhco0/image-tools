#include <algorithm>
#include <ctype.h>
#include <string>
#include <vcruntime_string.h>
#include <vector>

#include <cxxopts.hpp>
#include <fmt/format.h>

#include "libbmp.h"

enum class Command { kUnkown = 0, kHistogram, kEqualization };

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

  if (command == "equalization") {
    return Command::kEqualization;
  }

  return Command::kUnkown;
}

RGBHistogram GetHistogram(BmpImg &img) {
  RGBHistogram histogram;
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

BmpImg CreateHistogramImage(const RGBHistogram &histogram) {
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
             red_rect.y + graph_height * (histogram.red[i] / (1.0 * max_red)), red);
  }
  for (int i = 0; i < 256; i++) {
    DrawLine(graph, green_rect.x + i, green_rect.y, green_rect.x + i,
             green_rect.y + graph_height * (histogram.green[i] / (1.0 * max_green)), green);
  }
  for (int i = 0; i < 256; i++) {
    DrawLine(graph, blue_rect.x + i, blue_rect.y, blue_rect.x + i,
             blue_rect.y + graph_height * (histogram.blue[i] / (1.0 * max_blue)), blue);
  }

  return graph;
}

void Equalize(const BmpImg &img) {}

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
  std::string ouput_bmp = result["output"].as<std::string>();

  BmpImg input_image;

  input_image.read(input_bmp);

  fmt::print("Using args: {} {} {}\n", input_bmp, method, ouput_bmp);

  switch (CommandByMethod(method)) {
    using enum Command;
  case kUnkown: {
    fmt::print("Unkown command\n");
    return 1;
  } break;

  case kHistogram: {
    RGBHistogram histogram = GetHistogram(input_image);

    BmpImg image = CreateHistogramImage(histogram);

    image.write(ouput_bmp);
  } break;

  case kEqualization: {

  } break;
  default:
    break;
  }

  return 0;
}