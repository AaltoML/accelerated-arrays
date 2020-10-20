#include <catch2/catch.hpp>
#include "opencv_adapter.hpp"

TEST_CASE( "OpenCV adapter (from)", "[accelerated-arrays]" ) {
    using namespace accelerated;

    cv::Mat testMat(3, 4, CV_8UC1);
    testMat = cv::Scalar(0);

    testMat.at<std::uint8_t>(1, 2) = 3;

    {
        auto image = opencv::ref(testMat);
        REQUIRE(image->get<std::uint8_t>(2, 1) == 3);
        REQUIRE(image->get<std::uint8_t>(1, 2) == 0);
        image->set<float>(1, 2, 4.001);
        REQUIRE(testMat.at<std::uint8_t>(2, 1) == 4);
    }

    {
        auto other = opencv::ref(testMat, true);
        REQUIRE(other->get<FixedPoint<std::uint8_t>>(1, 2).value == 4);
    }

    {
        auto img = opencv::emptyLike(testMat);
        opencv::copy(testMat, *img);
        REQUIRE(img->get<std::uint8_t>(2, 1) == 3);
    }
}

TEST_CASE( "OpenCV adapter (to)", "[accelerated-arrays]" ) {
    using namespace accelerated;

    auto factory = cpu::Image::createFactory();
    auto testImg = factory->create<float, 2>(12, 11);

    cv::Mat mat = opencv::ref(*testImg);

    auto &cpuImg = cpu::Image::castFrom(*testImg);

    cpuImg.set<float>(4, 5, 1, 123.4);
    REQUIRE(std::fabs(mat.at<cv::Vec2f>(5, 4)(1) - 123.4) < 1e-5);

    cv::Mat copyMat;
    opencv::copy(cpuImg, copyMat);
    REQUIRE(std::fabs(copyMat.at<cv::Vec2f>(5, 4)(1) - 123.4) < 1e-5);
}
