/*******************************************************************************
*  The "New BSD License" : http://www.opensource.org/licenses/bsd-license.php  *
********************************************************************************

Copyright (c) 2010, Mark Turney, Copyright (c) 2020-2021, Adrian Böckenkamp
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the <organization> nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

******************************************************************************/

#ifndef SVG_WRITER_HPP
#define SVG_WRITER_HPP

#include <vector>
#include <string>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <memory>
#include <set>
#include <cmath>
#include <stdexcept>

#include <iostream>

namespace svg {

// Version information.
inline std::string libraryName()    { return "svg-writer"; }
inline std::string libraryVersion() { return "1.0.0"; }
inline std::string svgVersion()     { return "1.1"; }

// Utility XML/String Functions.
template <typename T>
inline std::string attribute(std::string const & attribute_name,
    T const & value, std::string const & unit = "")
{
    std::stringstream ss;
    ss << attribute_name << "=\"" << value << unit << "\" ";
    return ss.str();
}
inline std::string elemStart(std::string const & element_name, bool single = false)
{
    return "\t<" + element_name + (single ? ">\n" : " ");
}
inline std::string elemEnd(std::string const & element_name)
{
    return "</" + element_name + ">\n";
}
inline std::string emptyElemEnd()
{
    return "/>\n";
}

template<typename T, typename... Args>
std::unique_ptr<T> make_unique(Args&&... args) {
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

inline bool ends_with(std::string const & value, std::string const & ending)
{
    if (ending.size() > value.size()) {
        return false;
    }
    return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
}

inline bool valid_num(double x) { return !std::isinf(x) && !std::isnan(x); }

inline bool equal(double a, double b, double eps = 1e-10) { return std::fabs(a - b) < eps; }

// Quick optional return type.  This allows functions to return an invalid
//  value if no good return is possible.  The user checks for validity
//  before using the returned value.
template <typename T>
class optional {
public:
    optional<T>(T const & value) : valid(true), type(value) { }
    optional<T>() : valid(false), type(T()) { }
    T* operator->()
    {
        // If we try to access an invalid value, an exception is thrown.
        if (!valid) {
            throw std::runtime_error("Invalid value in svg::optional accessed.");
        }

        return &type;
    }
    const T* operator->() const
    {
        // If we try to access an invalid value, an exception is thrown.
        if (!valid) {
            throw std::runtime_error("Invalid (const) value in svg::optional accessed.");
        }

        return &type;
    }
    // Test for validity.
    bool operator!() const { return !valid; }
    operator bool() const { return valid; }
private:
    bool valid;
    T type;
};

struct Dimensions {
    Dimensions(double w, double h) : width(w), height(h)
    {
        if (!valid_num(width) || !valid_num(height)) {
            std::cerr << "Infs or NaNs provided to svg::Dimensions()." << std::endl;
        }
    }
    Dimensions(double combined = 0) : width(combined), height(combined)
    {
      if (!valid_num(combined)) {
          std::cerr << "Infs or NaNs provided to svg::Dimensions()." << std::endl;
      }
    }
    double width;
    double height;
};

struct Point {
    Point(double px = 0, double py = 0) : x(px), y(py) { }
    double x;
    double y;
};

inline optional<Point> getMinPoint(std::vector<Point> const & points)
{
    if (points.empty()) {
        return {};
    }

    Point min = points[0];
    for (unsigned i = 0; i < points.size(); ++i) {
        if (points[i].x < min.x) {
            min.x = points[i].x;
        }
        if (points[i].y < min.y) {
            min.y = points[i].y;
        }
    }
    return optional<Point>(min);
}

inline optional<Point> getMaxPoint(std::vector<Point> const & points)
{
    if (points.empty()) {
        return {};
    }

    Point max = points[0];
    for (unsigned i = 0; i < points.size(); ++i) {
        if (points[i].x > max.x) {
            max.x = points[i].x;
        }
        if (points[i].y > max.y) {
            max.y = points[i].y;
        }
    }
    return optional<Point>(max);
}

// Defines the dimensions, scale, origin, and origin offset of the document.
struct Layout {
    enum Origin { TopLeft, BottomLeft, TopRight, BottomRight };

    Layout(Dimensions const & dims = Dimensions(400, 300), Origin orig = BottomLeft,
        double dim_scale = 1, Point const & orig_offset = Point(0, 0))
        : dimensions(dims), scale(dim_scale), origin(orig), origin_offset(orig_offset)
    {
      if (!valid_num(scale) || !valid_num(origin_offset.x) || !valid_num(origin_offset.y)) {
          std::cerr << "Infs or NaNs provided to svg::Layout()." << std::endl;
      }
    }
    Dimensions dimensions;
    double scale;
    Origin origin;
    Point origin_offset;
};

// Convert coordinates in user space to SVG native space.
inline double translateX(double x, Layout const & layout)
{
    if (layout.origin == Layout::BottomRight || layout.origin == Layout::TopRight) {
        return layout.dimensions.width - ((x + layout.origin_offset.x) * layout.scale);
    } else {
        return (layout.origin_offset.x + x) * layout.scale;
    }
}

inline double translateY(double y, Layout const & layout)
{
    if (layout.origin == Layout::BottomLeft || layout.origin == Layout::BottomRight) {
        return layout.dimensions.height - ((y + layout.origin_offset.y) * layout.scale);
    } else {
        return (layout.origin_offset.y + y) * layout.scale;
    }
}
inline double translateScale(double dimension, Layout const & layout)
{
    return dimension * layout.scale;
}

class Serializeable {
public:
    Serializeable() { }
    virtual ~Serializeable() { }
    virtual std::string toString(Layout const & l) const = 0;
};

class Identifiable {
public:
  Identifiable(const std::string &identifier = {}) : id(identifier) { }
  virtual ~Identifiable() { }
  const std::string& getId() const { return id; }
  void setId(const std::string &new_id = {}) { id = new_id; }
  static std::string random(size_t len = 8)
  {
      std::string tmp_s;
      static const char alphanum[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

      static bool init_once = false;
      if (!init_once) { // one-shot initialization upon first entering
        std::srand(unsigned(time(nullptr)));
        init_once = true;
      }
      tmp_s.reserve(len);
      for (size_t i = 0; i < len; ++i) {
          tmp_s += alphanum[unsigned(rand()) % (sizeof(alphanum) - 1)];
      }
      return tmp_s;
  }
protected:
  std::string id;
  std::string serializeId() const
  {
    if (id.empty()) {
      return {};
    } else {
      return attribute("id", id);
    }
  }
};

class Color : public Serializeable {
public:
    enum Defaults { Transparent = -1, Aqua, Black, Gray, Blue, Brown, Cyan, Fuchsia,
        Green, Lime, Magenta, Orange, Purple, Red, Silver, White, Yellow, Random };

    Color(unsigned char r, unsigned char g, unsigned char b) : transparent(false), red(r), green(g), blue(b) { }
    Color(Defaults color)
        : transparent(false), red(0), green(0), blue(0)
    {
        switch (color) {
        case Aqua:    assign(  0, 255, 255); break;
        case Black:   assign(  0,   0,   0); break;
        case Gray:    assign(127, 127, 127); break;
        case Blue:    assign(  0,   0, 255); break;
        case Brown:   assign(165,  42,  42); break;
        case Cyan:    assign(  0, 255, 255); break;
        case Fuchsia: assign(255,   0, 255); break;
        case Green:   assign(  0, 128,   0); break;
        case Lime:    assign(  0, 255,   0); break;
        case Magenta: assign(255,   0, 255); break;
        case Orange:  assign(255, 165,   0); break;
        case Purple:  assign(128,   0, 128); break;
        case Red:     assign(255,   0,   0); break;
        case Silver:  assign(192, 192, 192); break;
        case White:   assign(255, 255, 255); break;
        case Yellow:  assign(255, 255,   0); break;
        case Random:
          static bool srand_init = false;
          if (!srand_init) {
              srand(static_cast<unsigned int>(time(nullptr)));
              srand_init = true;
          }
          assign(rand() % 256, rand() % 256, rand() % 256);
          break;
        case Transparent: // fall through...
        default: transparent = true; break;
        }
    }
    virtual ~Color() { }
    std::string toString(Layout const &) const
    {
        std::stringstream ss;
        if (transparent) {
            ss << "none";
        } else {
            ss << "rgb(" << red << "," << green << "," << blue << ")";
        }
        return ss.str();
    }
private:
    bool transparent;
    int red;
    int green;
    int blue;

    void assign(int r, int g, int b) { red = r; green = g; blue = b; }
};

class Fill : public Serializeable {
public:
    Fill(Color::Defaults fill_color, double opacity_level = 1.0)
        : color(fill_color), opacity(opacity_level)
    {
        if (opacity_level < 0 || opacity_level > 1) {
            std::cerr << "Fill::Fill(): opacity_level=" << opacity_level << " is out of range [0,1]." << std::endl;
        }
    }
    Fill(Color fill_color = Color::Transparent)
        : color(fill_color), opacity(1.0) { }
    std::string toString(Layout const & l) const
    {
        std::stringstream ss;
        ss << attribute("fill", color.toString(l));
        if (opacity < 1.0) {
            ss << attribute("fill-opacity", opacity);
        }
        return ss.str();
    }
private:
    Color color;
    double opacity; // in [0, 1], 1 = fully visible, 0 = fully transparent
};

class Stroke : public Serializeable {
public:
    Stroke(double w = -1, Color stroke_color = Color::Transparent, bool nonScalingStroke = false,
           double stroke_miterlimit = -1, std::vector<unsigned> stroke_dasharray = {},
           unsigned int stroke_dashoffset = 0, double stroke_opacity = 1.0)
        : width(w), color(stroke_color), nonScaling(nonScalingStroke),
          miterlimit(stroke_miterlimit), dasharray(stroke_dasharray),
          dashoffset(stroke_dashoffset), opacity(stroke_opacity)
    {
        if (!valid_num(width) || !valid_num(stroke_miterlimit) ||
            !valid_num(stroke_opacity)) {
            std::cerr << "Infs or NaNs provided to svg::Stroke()." << std::endl;
        }
        if (stroke_opacity < 0 || stroke_opacity > 1) {
            std::cerr << "Stroke::Stroke(): stroke_opacity=" << stroke_opacity << " is out of range [0,1]." << std::endl;
        }
    }
    std::string toString(Layout const & l) const
    {
        // If stroke width is invalid.
        if (width < 0) {
            return std::string();
        }

        std::stringstream ss;
        ss << attribute("stroke-width", translateScale(width, l))
           << attribute("stroke", color.toString(l));
        if (miterlimit >= 0) {
            ss << attribute("stroke-miterlimit", translateScale(miterlimit, l));
        }
        ss << attribute("stroke-dashoffset", translateScale(dashoffset, l));
        std::stringstream tmp;
        for (size_t i = 0; i < dasharray.size(); ++i) {
            tmp << dasharray[i];
            if (i + 1 < dasharray.size()) {
                tmp << ",";
            }
        }
        if (!dasharray.empty()) {
            ss << attribute("stroke-dasharray", tmp.str());
        }
        if (opacity < 1.0) {
            ss << attribute("stroke-opacity", opacity);
        }
        if (nonScaling) {
           ss << attribute("vector-effect", "non-scaling-stroke");
        }
        return ss.str();
    }
private:
    double width;
    Color color;
    bool nonScaling;
    double miterlimit;
    std::vector<unsigned> dasharray;
    unsigned dashoffset;
    double opacity; // in [0, 1], 1 = fully visible, 0 = fully transparent
};

class Font : public Serializeable {
public:
    Font(double font_size = 12, std::string const & font_family = "Verdana")
        : size(font_size), family(font_family) { }
    std::string toString(Layout const & l) const
    {
        std::stringstream ss;
        ss << attribute("font-size", translateScale(size, l)) << attribute("font-family", family);
        return ss.str();
    }
    double getSize() const { return size; }
    void setSize(double s) { size = s; }
    const std::string &getFamily() const { return family; }
    void setFamily(const std::string &f) { family = f; }
private:
    double size;
    std::string family;
};

// All SVG entities (shapes) than have a stroke (that is, Line, Polyline, and all listed for "SurfaceShape")
class Shape : public Serializeable, public Identifiable {
public:
    Shape(Stroke const & stroke_style = Stroke(), int z_order = 0, const std::string& shape_id = {})
        : Identifiable(shape_id), z(z_order), stroke(stroke_style) { }
    virtual ~Shape() { }
    std::string toString(Layout const & l) const override
    {
        std::stringstream ss;
        ss << stroke.toString(l);
        if (!style.empty()) {
            ss << attribute("style", style);
        }
        if (!visible) {
            ss << attribute("visibility", "hidden");
        }
        return ss.str();
    }
    virtual void offset(Point const & offset) = 0;
    virtual std::unique_ptr<Shape> clone() const = 0;
    Stroke getStroke() const { return stroke; }
    const std::string& getStyle() const { return style; }
    void setStroke(Stroke s) { stroke = s; }
    void setStyle(const std::string &new_style) { style = new_style; }
    bool isVisible() const { return visible; }
    void hide() { visible = false; }
    void show() { visible = true; }
    /**
     * z order of SVG elements in the document. Default is zero which equals the order of insertion, that is,
     * an element A that is inserted after an element B overlays it because A is drawn after (and possibly over) B.
     * If all elements are zero, the default library behavior is applied (according to the SVG standard) and
     * elements are not reordered before they are written to the SVG file.
     * Elements (aka Shape's) with a smaller (and also possibly a negative) `z` are added/drawn before others with a larger z.
     * That means, if you want an element A to be above/over an element B, set something like A.z > B.z. Alternatively, set
     * A.z = B.z (or simply don't change it at all but then you have to ensure to add B *first*, then A.
     */
    int z;
protected:
    Stroke stroke;
    std::string style;
    bool visible = true;
};

// All SVG entities (shapes) that can be filled (that is, Circle, Ellipse, Rectangle, Polygon, Path, and Text)
class SurfaceShape : public Shape {
public:
    SurfaceShape(Fill const & fill_style = Fill(), Stroke const & stroke_style = Stroke(), int z_order = 0, const std::string& shape_id = {})
        : Shape(stroke_style, z_order, shape_id), fill(fill_style) { }
    std::string toString(Layout const & l) const override
    {
        return Shape::toString(l) + fill.toString(l);
    }
    void setFill(Fill f) { fill = f; }
    Fill getFill() const { return fill; }
protected:
    Fill fill;
    virtual ~SurfaceShape() { }
};

class Marker : public Serializeable, public Identifiable {
public:
    // Creates an empty marker (no visual effect).
    Marker() : orient("auto") { }
    Marker(const std::string &markerId, double markerWidth, double markerHeight, double refX, double refY,
           const Shape &shape, const std::string &orientation = "auto")
        : Identifiable(markerId), marker_width(markerWidth), marker_height(markerHeight), ref_x(refX),
          ref_y(refY), orient(orientation)
    {
        *this << shape;
    }
    Marker(const Marker &that) : Identifiable(that.id)
    {
        shapes.reserve(that.shapes.size());
        for (size_t i = 0; i < that.shapes.size(); ++i) {
            shapes.push_back(that.shapes[i]->clone());
        }
        id = that.id;
        marker_width = that.marker_width;
        marker_height = that.marker_height;
        ref_x = that.ref_x;
        ref_y = that.ref_y;
        orient = that.orient;
    }
    Marker(Marker &&that)
    {
        shapes.reserve(that.shapes.size());
        for (size_t i = 0; i < that.shapes.size(); ++i) {
            shapes.push_back(std::move(that.shapes[i]));
        }
        that.shapes.clear();

        id = that.id;
        that.id.clear();
        marker_width = that.marker_width;
        marker_height = that.marker_height;
        ref_x = that.ref_x;
        ref_y = that.ref_y;
        orient = that.orient;
    }
    Marker& operator=(Marker &that)
    {
        if (this != &that) {
            shapes.reserve(that.shapes.size());
            for (size_t i = 0; i < that.shapes.size(); ++i) {
                shapes.push_back(that.shapes[i]->clone());
            }
            id = that.id;
            marker_width = that.marker_width;
            marker_height = that.marker_height;
            ref_x = that.ref_x;
            ref_y = that.ref_y;
            orient = that.orient;
        }
        return *this;
    }
    Marker& operator=(Marker &&that)
    {
        if (this != &that) {
            shapes.reserve(that.shapes.size());
            for (size_t i = 0; i < that.shapes.size(); ++i) {
                shapes.push_back(std::move(that.shapes[i]));
            }
            that.shapes.clear();

            id = that.id;
            that.id.clear();
            marker_width = that.marker_width;
            marker_height = that.marker_height;
            ref_x = that.ref_x;
            ref_y = that.ref_y;
            orient = that.orient;
        }
        return *this;
    }
    Marker& operator<<(const Shape &shape)
    {
        shapes.push_back(shape.clone());
        return *this;
    }
    std::string toString(Layout const &) const override
    {
        if (id.empty()) {
            throw std::invalid_argument("svg::Marker::toString() requires a non-empty ID to refer to that marker.");
        }

        // Don't add any translation:
        const Layout UNCHANGED(Dimensions(), Layout::TopLeft);

        std::stringstream ss;
        if (valid()) { // only if not empty / defined
            ss << "\t" << elemStart("marker")
               << serializeId()
               << attribute("markerWidth", marker_width)
               << attribute("markerHeight", marker_height)
               << attribute("refX", ref_x)
               << attribute("refY", ref_y)
               << attribute("orient", orient) << ">\n";
            for (size_t i = 0; i < shapes.size(); ++i) {
                ss << "\t\t" << shapes[i]->toString(UNCHANGED);
                if (i + 1 < shapes.size()) {
                    ss << "\n";
                }
            }
            ss << "\t\t" << elemEnd("marker");
        }
        return ss.str();
    }
    bool valid() const { return !id.empty(); }
    std::unique_ptr<Shape>& operator[](size_t index) { return shapes[index]; }
    size_t size() const { return shapes.size(); }
    // Checks if \c *this is \a visually not equal to \c that. \c id is \a not considered.
    // This is used in svg::Document to test for Marker ID collisions.
    bool operator!=(const Marker &that) const
    {
        if (shapes.size() != that.shapes.size() || !equal(marker_width, that.marker_width) ||
            !equal(ref_x, that.ref_x) || !equal(marker_height, that.marker_height) ||
            !equal(ref_y, that.ref_y)) {
            return true;
        }
        // Convert all shapes to strings, sort them to make the comparison independent of the
        // order:
        const Layout DUMMY;
        std::vector<std::string> str_shapes[2];
        str_shapes[0].resize(shapes.size());
        str_shapes[1].resize(shapes.size());
        for (size_t i = 0; i < shapes.size(); ++i) {
            str_shapes[0][i] = shapes[i]->toString(DUMMY);
            str_shapes[1][i] = that.shapes[i]->toString(DUMMY);
        }
        std::sort(str_shapes[0].begin(), str_shapes[0].end());
        std::sort(str_shapes[1].begin(), str_shapes[1].end());
        for (size_t i = 0; i < shapes.size(); ++i) {
            if (str_shapes[0][i] != str_shapes[1][i]) {
                return true;
            }
        }
        return false;
    }
    void setOrientation(const std::string orientation = "auto")
    {
        if (orientation != "auto" && orientation != "auto-start-reverse") {
            throw std::invalid_argument("Marker::setOrientation() only accepts the strings \"auto\" or \"auto-start-reverse\".");
        }
        orient = orientation;
    }
    void setOrientation(double angle) { orient = std::to_string(angle); }
private:
    std::vector<std::unique_ptr<Shape>> shapes;
    double marker_width;
    double marker_height;
    double ref_x;
    double ref_y;
    std::string orient;
};

namespace internal {
    inline auto compareMarker = [](const Marker *a, const Marker *b) { return a->getId() < b->getId(); };
    typedef std::set<const Marker*, decltype(compareMarker)> MarkerSet;
}

class Markerable : public Serializeable {
public:
    Markerable() : marker_start(nullptr), marker_mid(nullptr), marker_end(nullptr) { }
    void setStartMarker(const Marker *m) { marker_start = m; }
    void setMidMarker(const Marker *m) { marker_mid = m; }
    void setEndMarker(const Marker *m) { marker_end = m; }
    std::string toString(Layout const &) const override
    {
        std::stringstream ss;
        if (marker_start && marker_start->valid()) {
            ss << "marker-start=\"url(#" << marker_start->getId() << ")\" ";
        }
        if (marker_mid && marker_mid->valid()) {
            ss << "marker-mid=\"url(#" << marker_mid->getId() << ")\" ";
        }
        if (marker_end && marker_end->valid()) {
            ss << "marker-end=\"url(#" << marker_end->getId() << ")\" ";
        }
        return ss.str();
    }
    internal::MarkerSet getUsedMarkers() const
    {
        internal::MarkerSet result(internal::compareMarker);
        if (marker_start && marker_start->valid()) {
            result.insert(marker_start);
        }
        if (marker_mid && marker_mid->valid()) {
            result.insert(marker_mid);
        }
        if (marker_end && marker_end->valid()) {
            result.insert(marker_end);
        }
        return result;
    }

private:
    const Marker *marker_start;
    const Marker *marker_mid;
    const Marker *marker_end;
};

template <typename T>
inline std::string vectorToString(std::vector<T> collection, Layout const & layout)
{
    std::string combination_str;
    for (unsigned i = 0; i < collection.size(); ++i) {
        combination_str += collection[i].toString(layout);
    }

    return combination_str;
}

class Circle : public SurfaceShape {
public:
    Circle(Point const & center_pos, double diameter, Fill const & fill_style,
           Stroke const & stroke_style = Stroke())
        : SurfaceShape(fill_style, stroke_style), center(center_pos), radius(diameter / 2)
    {
        if (!valid_num(center.x) || !valid_num(center.y) || !valid_num(diameter)) {
            std::cerr << "Infs or NaNs provided to svg::Circle()." << std::endl;
        }
    }
    std::string toString(Layout const & l) const override
    {
        std::stringstream ss;
        ss << elemStart("circle") << serializeId()
           << attribute("cx", translateX(center.x, l))
           << attribute("cy", translateY(center.y, l))
           << attribute("r", translateScale(radius, l))
           << SurfaceShape::toString(l) << emptyElemEnd();
        return ss.str();
    }
    void offset(Point const & offset) override
    {
        if (!valid_num(offset.x) || !valid_num(offset.y)) {
            std::cerr << "Infs or NaNs provided to svg::Circle::offset()." << std::endl;
        }
        center.x += offset.x;
        center.y += offset.y;
    }
    std::unique_ptr<Shape> clone() const override
    {
        return svg::make_unique<Circle>(*this);
    }
private:
    Point center;
    double radius;
};

class Elipse : public SurfaceShape {
public:
    Elipse(Point const & center_pos, double width, double height,
        Fill const & fill_style = Fill(), Stroke const & stroke_style = Stroke())
        : SurfaceShape(fill_style, stroke_style), center(center_pos), radius_width(width / 2.0),
          radius_height(height / 2.0)
    {
        if (!valid_num(center.x) || !valid_num(center.y) || !valid_num(width) || !valid_num(height)) {
            std::cerr << "Infs or NaNs provided to svg::Elipse()." << std::endl;
        }
    }
    std::string toString(Layout const & l) const override
    {
        std::stringstream ss;
        ss << elemStart("ellipse")<< serializeId()
           << attribute("cx", translateX(center.x, l))
           << attribute("cy", translateY(center.y, l))
           << attribute("rx", translateScale(radius_width, l))
           << attribute("ry", translateScale(radius_height, l))
           << SurfaceShape::toString(l) << emptyElemEnd();
        return ss.str();
    }
    void offset(Point const & offset) override
    {
        if (!valid_num(offset.x) || !valid_num(offset.y)) {
            std::cerr << "Infs or NaNs provided to svg::Elipse::offset()." << std::endl;
        }
        center.x += offset.x;
        center.y += offset.y;
    }
    std::unique_ptr<Shape> clone() const override
    {
        return svg::make_unique<Elipse>(*this);
    }
private:
    Point center;
    double radius_width;
    double radius_height;
};

class Rectangle : public SurfaceShape {
public:
    /**
     * Creates a rectangle (fillable shape).
     * \param [in] edge Upper left corner of the rectangle
     * \param [in] width Width of the rectangle
     * \param [in] height Height of the rectangle
     * \param [in] fill Fill style used to fill the rectangular area
     * \param [in] stroke Stroke used to create the boundary contour
     */
    Rectangle(Point const & upper_left_corner, double w, double h, Fill const & fill_style = Fill(),
              Stroke const & stroke_style = Stroke(), double rx_corner = 0.0, double ry_corner = 0.0)
        : SurfaceShape(fill_style, stroke_style), edge(upper_left_corner), width(w), height(h), rx(rx_corner),
          ry(ry_corner)
    {
        if (!valid_num(edge.x) || !valid_num(edge.y) || !valid_num(width) || !valid_num(height) ||
            !valid_num(rx) || !valid_num(ry)) {
            std::cerr << "Infs or NaNs provided to svg::Rectangle()." << std::endl;
        }
    }
    std::string toString(Layout const & l) const override
    {
        std::stringstream ss;
        ss << elemStart("rect") << serializeId()
           << attribute("x", translateX(edge.x, l))
           << attribute("y", translateY(edge.y, l));
        if (rx > 0.0 || ry > 0.0) {
            ss << attribute("rx", rx) << attribute("ry", ry);
        }
        ss << attribute("width", translateScale(width, l))
           << attribute("height", translateScale(height, l))
           << SurfaceShape::toString(l) << emptyElemEnd();
        return ss.str();
    }
    void offset(Point const & offset) override
    {
        if (!valid_num(offset.x) || !valid_num(offset.y)) {
            std::cerr << "Infs or NaNs provided to svg::Rectangle::offset()." << std::endl;
        }
        edge.x += offset.x;
        edge.y += offset.y;
    }
    Rectangle centerAt(Point const & pos) const
    {
        if (!valid_num(pos.x) || !valid_num(pos.y)) {
            std::cerr << "Infs or NaNs provided to svg::Rectangle::centerAt()." << std::endl;
        }
        return Rectangle(Point(pos.x - width / 2.0, pos.y - height / 2.0), width, height, fill, stroke);
    }
    std::unique_ptr<Shape> clone() const override
    {
        return svg::make_unique<Rectangle>(*this);
    }
private:
    Point edge;
    double width;
    double height;
    double rx; //<! Rounded corner radius in x direction
    double ry; //<! Rounded corner radius in y direction
};

class Line : public Shape, public Markerable {
public:
    Line(Point const & start_pt, Point const & end_pt, Stroke const & stroke_style = Stroke())
        : Shape(stroke_style), start_point(start_pt), end_point(end_pt)
    {
        if (!valid_num(start_point.x) || !valid_num(start_point.y) ||
            !valid_num(end_point.x) || !valid_num(end_point.y)) {
            std::cerr << "Infs or NaNs provided to svg::Line()." << std::endl;
        }
    }
    std::string toString(Layout const & l) const override
    {
        std::stringstream ss;
        ss << elemStart("line") << serializeId()
           << attribute("x1", translateX(start_point.x, l))
           << attribute("y1", translateY(start_point.y, l))
           << attribute("x2", translateX(end_point.x, l))
           << attribute("y2", translateY(end_point.y, l))
           << Shape::toString(l) << Markerable::toString(l) << emptyElemEnd();
        return ss.str();
    }
    void offset(Point const & offset) override
    {
        if (!valid_num(offset.x) || !valid_num(offset.y)) {
            std::cerr << "Infs or NaNs provided to svg::Line::offset()." << std::endl;
        }
        start_point.x += offset.x;
        start_point.y += offset.y;

        end_point.x += offset.x;
        end_point.y += offset.y;
    }
    std::unique_ptr<Shape> clone() const override
    {
        return svg::make_unique<Line>(*this);
    }
private:
    Point start_point;
    Point end_point;
};

class Polygon : public SurfaceShape {
public:
    Polygon(Fill const & fill_style = Fill(), Stroke const & stroke_style = Stroke())
        : SurfaceShape(fill_style, stroke_style) { }
    Polygon(const std::vector<Point> &pts, Fill const & fill_style = Fill(), Stroke const & stroke_style = Stroke())
        : SurfaceShape(fill_style, stroke_style), points(pts)
    {
        for (size_t i = 0; i < pts.size(); ++i) {
            if (!valid_num(pts[i].x) || !valid_num(pts[i].y)) {
                std::cerr << "Infs or NaNs provided to svg::Polygon()." << std::endl;
                break;
            }
        }
    }
    Polygon(Stroke const & stroke_style = Stroke()) : SurfaceShape(Color::Transparent, stroke_style) { }
    Polygon & operator<<(Point const & point)
    {
        if (!valid_num(point.x) || !valid_num(point.y)) {
            std::cerr << "Infs or NaNs provided to svg::Polygon::operator<<()." << std::endl;
        }
        points.push_back(point);
        return *this;
    }
    std::string toString(Layout const & l) const override
    {
        std::stringstream ss;
        ss << elemStart("polygon") << serializeId();

        ss << "points=\"";
        for (decltype(points)::size_type i = 0; i < points.size(); ++i) {
            ss << translateX(points[i].x, l) << "," << translateY(points[i].y, l) << " ";
        }
        ss << "\" ";

        ss << SurfaceShape::toString(l) << emptyElemEnd();
        return ss.str();
    }
    void offset(Point const & offset) override
    {
        if (!valid_num(offset.x) || !valid_num(offset.y)) {
            std::cerr << "Infs or NaNs provided to svg::Polygon::offset()." << std::endl;
        }
        for (decltype(points)::size_type i = 0; i < points.size(); ++i) {
            points[i].x += offset.x;
            points[i].y += offset.y;
        }
    }
    std::unique_ptr<Shape> clone() const override
    {
        return svg::make_unique<Polygon>(*this);
    }
private:
    std::vector<Point> points;
};

class Path : public SurfaceShape {
public:
    Path(Fill const & fill_style = Fill(), Stroke const & stroke_style = Stroke())
        : SurfaceShape(fill_style, stroke_style)
    { startNewSubPath(); }
    Path(std::vector<Point> const & pts, Fill const & fill_style = Fill(), Stroke const & stroke_style = Stroke())
        : SurfaceShape(fill_style, stroke_style)
    {
        for (size_t i = 0; i < pts.size(); ++i) {
            if (!valid_num(pts[i].x) || !valid_num(pts[i].y)) {
                std::cerr << "Infs or NaNs provided to svg::Path()." << std::endl;
                break;
            }
        }
        startNewSubPath();
        paths.back() = pts;
    }
    Path(Stroke const & stroke_style = Stroke()) : SurfaceShape(Color::Transparent, stroke_style)
    {  startNewSubPath(); }
    Path & operator<<(Point const & point)
    {
        if (!valid_num(point.x) || !valid_num(point.y)) {
            std::cerr << "Infs or NaNs provided to svg::Path::operator<<()." << std::endl;
        }
        paths.back().push_back(point);
        return *this;
    }
    void startNewSubPath()
    {
        if (paths.empty() || 0 < paths.back().size()) {
            paths.emplace_back();
        }
    }
    std::string toString(Layout const & l) const override
    {
        std::stringstream ss;
        ss << elemStart("path") << serializeId();

        ss << "d=\"";
        for (auto const& subpath: paths) {
            if (subpath.empty()) {
                continue;
            }

            ss << "M";
            for (auto const& point: subpath) {
                ss << translateX(point.x, l) << "," << translateY(point.y, l) << " ";
            }
            ss << "z ";
      }
      ss << "\" ";
      ss << "fill-rule=\"evenodd\" ";

      ss << SurfaceShape::toString(l) << emptyElemEnd();
      return ss.str();
    }
    void offset(Point const & offset) override
    {
        if (!valid_num(offset.x) || !valid_num(offset.y)) {
            std::cerr << "Infs or NaNs provided to svg::Path::offset()." << std::endl;
        }
        for (auto& subpath : paths) {
           for (auto& point : subpath) {
                point.x += offset.x;
                point.y += offset.y;
            }
        }
    }
    std::unique_ptr<Shape> clone() const override
    {
        return svg::make_unique<Path>(*this);
    }
private:
    std::vector<std::vector<Point>> paths;
};

class Polyline : public Shape, public Markerable {
public:
    Polyline(Stroke const & stroke_style = Stroke()) : Shape(stroke_style) { }
    Polyline(std::vector<Point> const & pts, Stroke const & stroke_style = Stroke())
        : Shape(stroke_style), points(pts)
    {
        for (size_t i = 0; i < pts.size(); ++i) {
            if (!valid_num(pts[i].x) || !valid_num(pts[i].y)) {
                std::cerr << "Infs or NaNs provided to svg::Polyline()." << std::endl;
                break;
            }
        }
    }
    Polyline & operator<<(Point const & point)
    {
        if (!valid_num(point.x) || !valid_num(point.y)) {
            std::cerr << "Infs or NaNs provided to svg::Polyline::operator<<()." << std::endl;
        }
        points.push_back(point);
        return *this;
    }
    std::string toString(Layout const & l) const override
    {
        std::stringstream ss;
        ss << elemStart("polyline") << serializeId() << attribute("fill", "none");

        ss << "points=\"";
        for (unsigned i = 0; i < points.size(); ++i) {
            ss << translateX(points[i].x, l) << "," << translateY(points[i].y, l) << " ";
        }
        ss << "\" ";

        ss << Shape::toString(l) << Markerable::toString(l) << emptyElemEnd();
        return ss.str();
    }
    void offset(Point const & offset) override
    {
        if (!valid_num(offset.x) || !valid_num(offset.y)) {
            std::cerr << "Infs or NaNs provided to svg::Polyline::offset()." << std::endl;
        }
        for (unsigned i = 0; i < points.size(); ++i) {
            points[i].x += offset.x;
            points[i].y += offset.y;
        }
    }
    std::unique_ptr<Shape> clone() const override
    {
        return svg::make_unique<Polyline>(*this);
    }
    std::vector<Point> points;
};

// None will not create any extra SVG/XML and equals "Start" (the default).
enum class TextAnchor { Start, Middle, End, None };

// None equals "auto", see https://developer.mozilla.org/en-US/docs/Web/SVG/Attribute/dominant-baseline
enum class DominantBaseline { TextBottom, Alphabetic, Ideographic, Middle, Central, Mathematical, Hanging, TextTop, None };

class Text : public SurfaceShape {
public:
    /**
     * \brief Construct a new Text object which is x/y centered on default
     */
    Text(Point const & origin_pos, std::string const & text_content, Fill const & fill_style = Fill(),
         Font const & font_style = Font(), Stroke const & stroke_style = Stroke(),
         TextAnchor align = TextAnchor::Middle, DominantBaseline baseline = DominantBaseline::Middle)
        : SurfaceShape(fill_style, stroke_style), origin(origin_pos), content(text_content), font(font_style),
          anchor(align), dominant_baseline(baseline)
    {
        if (!valid_num(origin.x) || !valid_num(origin.y)) {
            std::cerr << "Infs or NaNs provided to svg::Text()." << std::endl;
        }
        if (content.empty()) {
            std::cerr << "Empty string provided to svg::Text()." << std::endl;
        }
    }
    std::string toString(Layout const & l) const override
    {
        std::stringstream ss;
        ss << elemStart("text") << serializeId();
        switch (anchor) {
        case TextAnchor::Start:
            ss << attribute("text-anchor", "start"); break;
        case TextAnchor::Middle:
            ss << attribute("text-anchor", "middle"); break;
        case TextAnchor::End:
            ss << attribute("text-anchor", "end"); break;
        case TextAnchor::None:
            break;
        }
        switch (dominant_baseline) {
        case DominantBaseline::TextBottom:
            ss << attribute("dominant-baseline", "text-bottom"); break;
        case DominantBaseline::Alphabetic:
            ss << attribute("dominant-baseline", "alphabetic"); break;
        case DominantBaseline::Ideographic:
            ss << attribute("dominant-baseline", "ideographic"); break;
        case DominantBaseline::Middle:
            ss << attribute("dominant-baseline", "middle"); break;
        case DominantBaseline::Central:
            ss << attribute("dominant-baseline", "central"); break;
        case DominantBaseline::Mathematical:
            ss << attribute("dominant-baseline", "mathematical"); break;
        case DominantBaseline::Hanging:
            ss << attribute("dominant-baseline", "hanging"); break;
        case DominantBaseline::TextTop:
            ss << attribute("dominant-baseline", "text-top"); break;
        case DominantBaseline::None:
            break;
        }
        ss << attribute("x", translateX(origin.x, l))
           << attribute("y", translateY(origin.y, l))
           << SurfaceShape::toString(l) << font.toString(l)
           << ">" << content << elemEnd("text");
        return ss.str();
    }
    void offset(Point const & offset) override
    {
        if (!valid_num(offset.x) || !valid_num(offset.y)) {
            std::cerr << "Infs or NaNs provided to svg::Text::offset()." << std::endl;
        }
        origin.x += offset.x;
        origin.y += offset.y;
    }
    std::unique_ptr<Shape> clone() const override
    {
        return svg::make_unique<Text>(*this);
    }
private:
    Point origin;
    std::string content;
    Font font;
    TextAnchor anchor;
    DominantBaseline dominant_baseline;
};

// TODO: allow "text with background" via filters, see https://stackoverflow.com/a/31013492

// Sample charting class.
// FIXME: this design is bad since "Polyline::points" needs to be public (in contrast to all other classes)...
class LineChart : public Shape {
public:
    LineChart(Dimensions chart_margin = Dimensions(),
              Stroke const & axis_stroke_style = Stroke(0.5, Color::Purple))
        : axis_stroke(axis_stroke_style), margin(chart_margin) { }
    LineChart & operator<<(Polyline const & polyline)
    {
        if (polyline.points.empty()) {
            return *this;
        }

        polylines.push_back(polyline);
        return *this;
    }
    std::string toString(Layout const & l) const override
    {
        if (polylines.empty()) {
            return "";
        }

        std::string ret;
        for (unsigned i = 0; i < polylines.size(); ++i) {
            ret += polylineToString(polylines[i], l);
        }

        return ret + axisString(l);
    }
    void offset(Point const & offset) override
    {
        if (!valid_num(offset.x) || !valid_num(offset.y)) {
            std::cerr << "Infs or NaNs provided to svg::LineChart::offset()." << std::endl;
        }
        for (unsigned i = 0; i < polylines.size(); ++i) {
            polylines[i].offset(offset);
        }
    }
    std::unique_ptr<Shape> clone() const override
    {
        return svg::make_unique<LineChart>(*this);
    }
private:
    Stroke axis_stroke;
    Dimensions margin;
    std::vector<Polyline> polylines;

    optional<Dimensions> getDimensions() const
    {
        if (polylines.empty()) {
            return optional<Dimensions>();
        }

        optional<Point> min = getMinPoint(polylines[0].points);
        optional<Point> max = getMaxPoint(polylines[0].points);
        for (unsigned i = 0; i < polylines.size(); ++i) {
            if (getMinPoint(polylines[i].points)->x < min->x)
                min->x = getMinPoint(polylines[i].points)->x;
            if (getMinPoint(polylines[i].points)->y < min->y)
                min->y = getMinPoint(polylines[i].points)->y;
            if (getMaxPoint(polylines[i].points)->x > max->x)
                max->x = getMaxPoint(polylines[i].points)->x;
            if (getMaxPoint(polylines[i].points)->y > max->y)
                max->y = getMaxPoint(polylines[i].points)->y;
        }

        return optional<Dimensions>(Dimensions(max->x - min->x, max->y - min->y));
    }
    std::string axisString(Layout const & layout) const
    {
        optional<Dimensions> dimensions = getDimensions();
        if (!dimensions) {
          return {};
        }

        // Make the axis 10% wider and higher than the data points.
        double width = dimensions->width * 1.1;
        double height = dimensions->height * 1.1;

        // Draw the axis.
        Polyline axis(axis_stroke);
        axis << Point(margin.width, margin.height + height) << Point(margin.width, margin.height)
             << Point(margin.width + width, margin.height);

        return axis.toString(layout);
    }
    std::string polylineToString(Polyline const & polyline, Layout const & layout) const
    {
        Polyline shifted_polyline = polyline;
        shifted_polyline.offset(Point(margin.width, margin.height));

        std::vector<Circle> vertices;
        for (unsigned i = 0; i < shifted_polyline.points.size(); ++i) {
            vertices.push_back(Circle(shifted_polyline.points[i], getDimensions()->height / 30.0, Color::Black));
        }

        return shifted_polyline.toString(layout) + vectorToString(vertices, layout);
    }
};

namespace animation {

class Animation : public Serializeable, public Identifiable {
public:
    Animation(const std::string &href_id, const std::string &ani_begin, const std::string &fill_style, const std::string &duration)
        : href(href_id), begin(ani_begin), fill(fill_style), dur(duration) { }
    std::string toString(Layout const &) const override
    {
        if (href.empty()) {
            std::cerr << "warning: no <href> given for animation with id=\"" << getId() << "\"." << std::endl;
        }
        std::stringstream ss;
        ss << serializeId() << attribute("href", "#" + href);
        if (!begin.empty()) {
            ss << attribute("begin", begin);
        }
        if (!fill.empty()) {
            ss << attribute("fill", fill);
        }
        if (!dur.empty()) {
            ss << attribute("dur", dur);
        }
        return ss.str();
    }
    virtual std::unique_ptr<Animation> clone() const = 0;
protected:
    std::string href;
    std::string begin;
    std::string fill;
    std::string dur;
};

class SetAttributeValue : public Animation {
public:
    SetAttributeValue(const std::string &ani_to, const std::string &attribute_name, const std::string &href_id,
                      const std::string &ani_begin = {}, const std::string &fill_style = {},
                      const std::string &duration = {}, const std::string attribute_type = "CSS")
        : Animation(href_id, ani_begin, fill_style, duration), to(ani_to), attr_name(attribute_name), attr_type(attribute_type) { }
    std::string toString(Layout const & l) const override
    {
        if (attr_name.empty()) {
            std::cerr << "warning: no <attributeName> given for animation with id=\"" << getId() << "\"." << std::endl;
        }
        std::stringstream ss;
        ss << elemStart("set") << Animation::toString(l) << attribute("to", to)
           << attribute("attributeName", attr_name)  << attribute("attributeType", attr_type)
           << emptyElemEnd();
        return ss.str();
    }
    std::unique_ptr<Animation> clone() const override
    {
        return svg::make_unique<SetAttributeValue>(*this);
    }
private:
    std::string to;
    std::string attr_name;
    std::string attr_type;
};

class AnimateMotion : public Animation {
public:
    AnimateMotion(std::vector<Point> pts, const std::string &href_id,
                  const std::string &ani_begin = {}, const std::string &fill_style = {},
                  const std::string &duration = {}) : Animation(href_id, ani_begin, fill_style, duration), points(pts) { }
    std::string toString(Layout const & l) const override
    {
        if (points.empty()) {
            std::cerr << "warning: no <path> points given as animation path for id=\"" << getId() << "\"." << std::endl;
        }
        std::stringstream ss;
        ss << elemStart("animateMotion") << Animation::toString(l) << "path=";
        for (size_t i = 0; i < points.size(); ++i) {
            if (i == 0) {
                ss << "\"M" << points[i].x << "," << points[i].y;
            } else {
                ss << "L" << points[i].x << "," << points[i].y;
            }
            if (i < points.size() - 1) {
                ss << " ";
            }
        }
        ss << "\" " << emptyElemEnd();
        return ss.str();
    }
    std::unique_ptr<Animation> clone() const override
    {
        return svg::make_unique<AnimateMotion>(*this);
    }
private:
    std::vector<Point> points;
};

} // end of namespace: animation (within namespace "svg")

class Document : public Identifiable {
public:
    Document(Layout doc_layout = Layout()) : layout(doc_layout), needs_sorting(false) { }

    Document & operator<<(Shape const & shape)
    {
        body_nodes.push_back(shape.clone());
        needs_sorting = needs_sorting || body_nodes.back()->z != 0;
        return *this;
    }
    Document & operator<<(animation::Animation const & animation)
    {
        animation_nodes.push_back(animation.clone());
        return *this;
    }
    std::string toString()
    {
        std::stringstream ss;
        writeToStream(ss);
        return ss.str();
    }
    bool isAnimated() const { return !animation_nodes.empty(); }
    /**
     * \brief Stores the SVG into a file on disk
     * \param [in] filename File name (may already contain desired extension)
     * \param [in] auto_append \c true to append the appropriate extension if not already present,
     *             \c false to not change `file_name`
     * \return \c true on success, \c false on failure
     * \see getFileName()
     */
    bool save(const std::string &filename, bool auto_append = true)
    {
        file_name = filename;
        // Append ".html" if not already given AND the document contains animations:
        if (auto_append) {
            if (isAnimated()) {
                if (!ends_with(file_name, ".html")) {
                    file_name += ".html";
                }
            } else if (!ends_with(file_name, ".svg")) {
                file_name += ".svg";
            }
        }
        std::ofstream ofs(file_name.c_str());
        if (!ofs.is_open()) {
            return false;
        }

        writeToStream(ofs);
        return ofs.good();
    }
    /**
     * \brief Returns the actual file name
     * \return file name incl. extension used in `save()`
     * \see save()
     */
    const std::string &getFileName() const { return file_name; }
    Layout getLayout() const { return layout; }
protected:
    void writeToStream(std::ostream& str)
    {
        str << "<?xml " << attribute("version", "1.0") << attribute("standalone", "no") << "?>\n"
            << "<!-- Generator: " << libraryName() << " (https://github.com/CodeFinder2/svg-writer), Version: " << libraryVersion() << " -->\n"
            << "<!DOCTYPE svg PUBLIC \"-//W3C//DTD SVG " << svgVersion() << "//EN\" "
            << "\"http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd\">\n<svg "
            << serializeId()
            << attribute("width", layout.dimensions.width, "px")
            << attribute("height", layout.dimensions.height, "px")
            << attribute("xmlns", "http://www.w3.org/2000/svg")
            << attribute("version", svgVersion()) << ">\n";
        if (needs_sorting) {
            // Note: animation nodes do not have to be sorted (order doesn't matter).
            std::stable_sort(body_nodes.begin(), body_nodes.end(),
                             [](const std::unique_ptr<Shape> &a, const std::unique_ptr<Shape> &b){
                // Ascending order rgd. z, keep equal z's (especially the default z=0) in the
                // order of insertions:
                return a->z < b->z;
            });
        }
        // Catch all markers and add them here if used:
        internal::MarkerSet all_used_markers(internal::compareMarker);
        for (const auto& body_node : body_nodes) {
            auto m = dynamic_cast<const Markerable*>(body_node.get());
            if (m) {
                auto markers = m->getUsedMarkers();
                for (const auto &i: markers) {
                    for (const auto &j: all_used_markers) {
                        if (i->getId() == j->getId() && *i != *j) {
                            std::cerr << "Marker collision detected for ID=" << i->getId()
                                      << " within this element: \n"
                                      << body_node->toString(layout)
                                      << "\nExpect markers not to be rendered correctly." << std::endl;
                        }
                    }
                    all_used_markers.insert(i);
                }
            }
        }
        if (!all_used_markers.empty()) {
            str << elemStart("defs", true);
            for (const auto &m: all_used_markers) {
                str << m->toString(layout);
            }
            str << "\t" << elemEnd("defs");
        }
        for (const auto& body_node : body_nodes) {
            str << body_node->toString(layout);
        }
        for (const auto& animation_node : animation_nodes) {
            str << animation_node->toString(layout);
        }
        str << elemEnd("svg");
    }
    std::string file_name;
    Layout layout;

    std::vector<std::unique_ptr<Shape>> body_nodes;
    bool needs_sorting;
    std::vector<std::unique_ptr<animation::Animation>> animation_nodes;
};

} // end of namespace: svg

#endif // SVG_WRITER_HPP
