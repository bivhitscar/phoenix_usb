#include "axis_calibration.h"

#include <cassert>
#include <filesystem>
#include <iostream>

int main()
{
    phoenix::AxisRange range;
    range.update(55);
    range.update(185);

    assert(range.normalize(55) == 0);
    assert(range.normalize(185) == 255);
    assert(range.normalize(120) == 127);
    assert(range.normalize(0) == 0);
    assert(range.normalize(255) == 255);

    phoenix::AxisRange degenerate;
    degenerate.update(100);
    degenerate.update(100);
    assert(degenerate.normalize(50) == 50);
    assert(degenerate.isDegenerate());

    phoenix::CalibrationSet set;
    set.x.update(40);
    set.x.update(200);
    set.y.update(30);
    set.y.update(220);
    set.rx.update(10);
    set.rx.update(240);
    set.ry.update(20);
    set.ry.update(230);
    set.rudder.update(50);
    set.rudder.update(210);
    set.throttle.update(60);
    set.throttle.update(200);
    set.aux2.update(70);
    set.aux2.update(190);
    set.aux3.update(80);
    set.aux3.update(170);

    const std::filesystem::path tempPath = std::filesystem::temp_directory_path() / L"phoenix_usb_calibration_test.ini";
    std::wstring error;
    assert(set.save(tempPath.wstring(), error));

    phoenix::CalibrationSet loaded;
    assert(loaded.load(tempPath.wstring(), error));
    assert(loaded.isValid());

    phoenix::CalibrationSet partial;
    partial.x.update(10);
    partial.x.update(200);
    partial.y.update(20);
    partial.y.update(210);
    partial.rx.update(30);
    partial.rx.update(220);
    partial.ry.update(90);
    partial.ry.update(90);
    partial.rudder.update(40);
    partial.rudder.update(200);
    partial.throttle.update(50);
    partial.throttle.update(210);
    partial.aux2.update(60);
    partial.aux2.update(190);
    partial.aux3.update(70);
    partial.aux3.update(180);

    assert(!partial.isValid());
    assert(partial.invalidAxisSummary().find(L"RY") != std::wstring::npos);
    partial.applyIdentityFallback();
    assert(partial.ry.min == 0 && partial.ry.max == 255);
    assert(partial.isValid());

    std::filesystem::remove(tempPath);

    std::cout << "axis calibration tests passed\n";
    return 0;
}
