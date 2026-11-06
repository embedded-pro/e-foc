#include "core/services/non_volatile_memory/LearnedModelData.hpp"
#include <array>
#include <gtest/gtest.h>

namespace
{
    using Image = std::array<uint8_t, services::learnedModelImageSize>;

    Image Serialized(const services::LearnedModelData& model)
    {
        Image image{};
        services::SerializeLearnedModel(model, infra::MakeRange(image));
        return image;
    }

    services::LearnedModelData Learned()
    {
        return services::LearnedModelData{ 0x12345678u, 1.25f, 4.5e-5f, std::nullopt };
    }
}

TEST(LearnedModelImage, the_image_fits_the_32_byte_region_cut_from_the_configuration_region)
{
    EXPECT_EQ(26u, services::learnedModelImageSize);
}

TEST(LearnedModelImage, a_learned_model_reads_back_as_it_was_stored)
{
    const auto image = Serialized(Learned());
    const auto read = services::DeserializeLearnedModel(infra::MakeConstByteRange(image));

    ASSERT_TRUE(read.has_value());
    EXPECT_EQ(Learned(), *read);
}

TEST(LearnedModelImage, an_empty_learned_model_reads_back_with_no_quantity)
{
    const auto image = Serialized(services::LearnedModelData{});
    const auto read = services::DeserializeLearnedModel(infra::MakeConstByteRange(image));

    ASSERT_TRUE(read.has_value());
    EXPECT_FALSE(read->inductance.has_value());
    EXPECT_FALSE(read->inertia.has_value());
    EXPECT_FALSE(read->viscousFriction.has_value());
}

TEST(LearnedModelImage, an_erased_region_is_not_a_learned_model)
{
    Image image{};
    image.fill(0xFF);

    EXPECT_FALSE(services::DeserializeLearnedModel(infra::MakeConstByteRange(image)).has_value());
}

TEST(LearnedModelImage, another_layout_version_is_not_a_learned_model)
{
    auto image = Serialized(Learned());
    image[4] = services::LearnedModelLayoutVersion + 1;

    EXPECT_FALSE(services::DeserializeLearnedModel(infra::MakeConstByteRange(image)).has_value());
}

TEST(LearnedModelImage, a_write_cut_short_is_not_a_learned_model)
{
    auto image = Serialized(Learned());
    image[12] ^= 0x01;

    EXPECT_FALSE(services::DeserializeLearnedModel(infra::MakeConstByteRange(image)).has_value());
}
