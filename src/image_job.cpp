#include "image_job.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize2.h"

#include <algorithm>
#include <cctype>
#include <fstream>

namespace fs = std::filesystem;

static std::string ext_lower(const fs::path& p) {
  auto e = p.extension().string();
  if (!e.empty() && e[0] == '.') e.erase(0, 1);
  for (auto& c : e) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return e;
}

static OutputFormat detect_keep(const fs::path& p) {
  const auto e = ext_lower(p);
  if (e == "jpg" || e == "jpeg") return OutputFormat::Jpeg;
  if (e == "png") return OutputFormat::Png;
  if (e == "webp") return OutputFormat::WebP;
  return OutputFormat::Png;
}

static std::string out_ext(OutputFormat f, const fs::path& in) {
  switch (f) {
    case OutputFormat::Jpeg: return "jpg";
    case OutputFormat::Png: return "png";
    case OutputFormat::WebP: return "webp";
    case OutputFormat::KeepOriginal: return ext_lower(in);
  }
  return "png";
}

static fs::path default_out_dir(const fs::path& input) {
  if (auto parent = input.parent_path(); !parent.empty()) return parent / "compressed";
  return fs::path("compressed");
}

static std::uint64_t fs_bytes(const fs::path& p) {
  std::error_code ec;
  const auto n = fs::file_size(p, ec);
  return ec ? 0 : n;
}

struct RgbaBuf {
  std::vector<unsigned char> pixels;
  int w = 0;
  int h = 0;
};

static bool load_rgba(const fs::path& path, RgbaBuf& out, std::string& err) {
  int comp = 0;
  unsigned char* data =
      stbi_load(path.string().c_str(), &out.w, &out.h, &comp, STBI_rgb_alpha);
  if (!data) {
    err = stbi_failure_reason() ? stbi_failure_reason() : "解码失败";
    return false;
  }
  out.pixels.assign(data, data + static_cast<size_t>(out.w * out.h * 4));
  stbi_image_free(data);
  return true;
}

static RgbaBuf resize_max_side(const RgbaBuf& src, int max_side) {
  if (max_side <= 0) return src;
  const int longest = std::max(src.w, src.h);
  if (longest <= max_side) return src;
  const float scale = static_cast<float>(max_side) / static_cast<float>(longest);
  const int nw = std::max(1, static_cast<int>(src.w * scale));
  const int nh = std::max(1, static_cast<int>(src.h * scale));
  RgbaBuf dst;
  dst.w = nw;
  dst.h = nh;
  dst.pixels.resize(static_cast<size_t>(nw * nh * 4));
  stbir_resize_uint8_linear(src.pixels.data(), src.w, src.h, 0, dst.pixels.data(), nw, nh, 0,
                            STBIR_RGBA);
  return dst;
}

static bool write_image(const fs::path& out, OutputFormat fmt, const RgbaBuf& img, int quality,
                        std::string& err) {
  const auto path = out.string();
  if (fmt == OutputFormat::WebP) {
    err = "C++ 版暂不支持 WebP 导出，请选 JPEG/PNG 或使用 Rust 版 qingtu";
    return false;
  }
  if (fmt == OutputFormat::Jpeg) {
    std::vector<unsigned char> rgb(static_cast<size_t>(img.w * img.h * 3));
    for (int i = 0; i < img.w * img.h; ++i) {
      rgb[i * 3] = img.pixels[i * 4];
      rgb[i * 3 + 1] = img.pixels[i * 4 + 1];
      rgb[i * 3 + 2] = img.pixels[i * 4 + 2];
    }
    if (!stbi_write_jpg(path.c_str(), img.w, img.h, 3, rgb.data(), quality)) {
      err = "写入 JPEG 失败";
      return false;
    }
    return true;
  }
  if (!stbi_write_png(path.c_str(), img.w, img.h, 4, img.pixels.data(), img.w * 4)) {
    err = "写入 PNG 失败";
    return false;
  }
  return true;
}

std::vector<JobResult> run_compress_jobs(const std::vector<fs::path>& inputs,
                                         const JobOptions& opts) {
  std::vector<JobResult> results;
  for (const auto& in : inputs) {
    JobResult r;
    r.input = in;
    r.bytes_before = fs_bytes(in);
    if (!fs::is_regular_file(in)) {
      r.message = "不是有效文件";
      results.push_back(std::move(r));
      continue;
    }
    OutputFormat fmt = opts.format;
    if (fmt == OutputFormat::KeepOriginal) fmt = detect_keep(in);
    const fs::path out_dir =
        opts.use_custom_out && !opts.output_dir.empty() ? opts.output_dir : default_out_dir(in);
    std::error_code ec;
    fs::create_directories(out_dir, ec);
    const fs::path out =
        out_dir / (in.stem().string() + "." + out_ext(fmt, in));

    std::string err;
    RgbaBuf buf;
    if (!load_rgba(in, buf, err)) {
      r.message = err;
      results.push_back(std::move(r));
      continue;
    }
    buf = resize_max_side(buf, opts.max_side);
    if (!write_image(out, fmt, buf, opts.quality, err)) {
      r.message = err;
      results.push_back(std::move(r));
      continue;
    }
    r.output = out;
    r.bytes_after = fs_bytes(out);
    r.message = "完成";
    results.push_back(std::move(r));
  }
  return results;
}

bool stitch_horizontal(const std::vector<fs::path>& inputs, const fs::path& output,
                       std::string& err) {
  if (inputs.size() < 2) {
    err = "拼接至少需要 2 张图片";
    return false;
  }
  std::vector<RgbaBuf> imgs;
  for (const auto& p : inputs) {
    RgbaBuf b;
    if (!load_rgba(p, b, err)) return false;
    imgs.push_back(std::move(b));
  }
  int target_h = imgs[0].h;
  for (const auto& im : imgs) target_h = std::min(target_h, im.h);
  std::vector<RgbaBuf> scaled;
  for (auto im : imgs) {
    if (im.h != target_h) {
      const int nw = std::max(1, im.w * target_h / im.h);
      RgbaBuf out;
      out.w = nw;
      out.h = target_h;
      out.pixels.resize(static_cast<size_t>(nw * target_h * 4));
      stbir_resize_uint8_linear(im.pixels.data(), im.w, im.h, 0, out.pixels.data(), nw, target_h,
                                0, STBIR_RGBA);
      im = std::move(out);
    }
    scaled.push_back(std::move(im));
  }
  int total_w = 0;
  for (const auto& im : scaled) total_w += im.w;
  RgbaBuf canvas;
  canvas.w = total_w;
  canvas.h = target_h;
  canvas.pixels.assign(static_cast<size_t>(total_w * target_h * 4), 0);
  int x = 0;
  for (const auto& im : scaled) {
    for (int y = 0; y < target_h; ++y) {
      memcpy(canvas.pixels.data() + static_cast<size_t>((y * total_w + x) * 4),
             im.pixels.data() + static_cast<size_t>(y * im.w * 4), static_cast<size_t>(im.w * 4));
    }
    x += im.w;
  }
  if (auto parent = output.parent_path(); !parent.empty()) fs::create_directories(parent);
  if (!stbi_write_png(output.string().c_str(), canvas.w, canvas.h, 4, canvas.pixels.data(), 6)) {
    err = "写入拼接结果失败";
    return false;
  }
  return true;
}
