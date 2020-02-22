#ifndef RESOURCES_H
#define RESOURCES_H

#include <array>

#include <GL/glew.h>

namespace ge1 {

    template<void (*Deleter)(GLuint)>
    struct unique_object {
        unique_object();
        unique_object(GLuint name);
        unique_object(const unique_object&) = delete;
        unique_object(unique_object&& other) noexcept;

        ~unique_object();

        unique_object& operator=(const unique_object&) = delete;
        unique_object& operator=(unique_object&& other) noexcept;

        GLuint get_name() const;

    private:
        GLuint name;
    };

    template<GLsizei Count, void (*Deleter)(GLsizei, GLuint*)>
    struct unique_objects {
        unique_objects();
        unique_objects(std::array<GLuint, Count> names);
        unique_objects(const unique_objects&) = delete;
        unique_objects(unique_objects&& other) noexcept;

        ~unique_objects();

        unique_objects& operator=(const unique_objects&) = delete;
        unique_objects& operator=(unique_objects&& other) noexcept;

        std::array<GLuint, Count> get_names() const;

    private:
        std::array<GLuint, Count> names;
    };


    template<void (*Deleter)(GLuint)>
    unique_object<Deleter>::unique_object() : name(0) {}

    template<void (*Deleter)(GLuint)>
    unique_object<Deleter>::unique_object(GLuint name) : name(name) {}

    template<void (*Deleter)(GLuint)>
    unique_object<Deleter>::unique_object(unique_object&& other) noexcept :
        name(other.name)
    {
        other.name = 0;
    }

    template<void (*Deleter)(GLuint)>
    unique_object<Deleter>::~unique_object() {
        if (name != 0) {
            Deleter(name);
        }
    }

    template<void (*Deleter)(GLuint)>
    unique_object<Deleter>& unique_object<Deleter>::operator=(
        unique_object<Deleter>&& other
    ) noexcept {
        if (name != 0) {
            Deleter(name);
        }
        name = other.name;
        other.name = 0;
    }

    template<void (*Deleter)(GLuint)>
    GLuint unique_object<Deleter>::get_name() const {
        return name;
    }

    template<GLsizei Count, void (*Deleter)(GLsizei, GLuint*)>
    unique_objects<Count, Deleter>::unique_objects() : names{0} {}

    template<GLsizei Count, void (*Deleter)(GLsizei, GLuint*)>
    unique_objects<Count, Deleter>::unique_objects(
        std::array<GLuint, Count> names
    ) : names(names) {}

    template<GLsizei Count, void (*Deleter)(GLsizei, GLuint*)>
    unique_objects<Count, Deleter>& unique_objects<Count, Deleter>::operator=(
        unique_objects<Count, Deleter>&& other
    ) noexcept {
        Deleter(Count, names);
        names = other.names;
        other.names = {0};
    }

    template<GLsizei Count, void (*Deleter)(GLsizei, GLuint*)>
    std::array<GLuint, Count> unique_objects<Count, Deleter>::get_names(
    ) const {
        return names;
    }

    template<GLsizei Count, void (*Deleter)(GLsizei, GLuint*)>
    unique_objects<Count, Deleter>::unique_objects(
        unique_objects<Count, Deleter>&& other
    ) noexcept : names(other.names) {
        other.names = {0};
    }

}

#endif // RESOURCES_H
