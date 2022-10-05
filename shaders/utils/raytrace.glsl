#pragma once

#define MAX_DIST 10000.0

struct Ray {
    f32vec3 o;
    f32vec3 nrm;
    f32vec3 inv_nrm;
};
struct IntersectionRecord {
    b32 hit;
    f32 internal_fac;
    f32 dist;
    f32vec3 nrm;
};
struct TraceRecord {
    IntersectionRecord intersection_record;
    f32vec3 color;
    u32 material;
    u32 object_i;
};
void default_init(out IntersectionRecord result) {
    result.hit = false;
    result.internal_fac = 1.0;
    result.dist = MAX_DIST;
    result.nrm = f32vec3(0, 0, 0);
}

Ray create_view_ray(f32vec2 uv) {
    Ray result;

    result.o = PLAYER.cam.pos;
    result.nrm = normalize(f32vec3(uv.x * PLAYER.cam.tan_half_fov, 1, -uv.y * PLAYER.cam.tan_half_fov));
    result.nrm = PLAYER.cam.rot_mat * result.nrm;
    result.inv_nrm = 1.0 / result.nrm;

    return result;
}

IntersectionRecord intersect(Ray ray, Sphere s) {
    IntersectionRecord result;
    default_init(result);

    f32vec3 so_r = ray.o - s.o;
    f32 a = dot(ray.nrm, ray.nrm);
    f32 b = 2.0f * dot(ray.nrm, so_r);
    f32 c = dot(so_r, so_r) - (s.r * s.r);
    f32 f = b * b - 4.0f * a * c;
    if (f < 0.0f)
        return result;
    result.internal_fac = (c > 0) ? 1.0 : -1.0;
    result.dist = (-b - sqrt(f) * result.internal_fac) / (2.0f * a);
    result.hit = result.dist > 0.0f;
    result.nrm = normalize(ray.o + ray.nrm * result.dist - s.o) * result.internal_fac;
    return result;
}
IntersectionRecord intersect(Ray ray, Box b) {
    IntersectionRecord result;
    default_init(result);

    f32 tx1 = (b.bound_min.x - ray.o.x) * ray.inv_nrm.x;
    f32 tx2 = (b.bound_max.x - ray.o.x) * ray.inv_nrm.x;
    f32 tmin = min(tx1, tx2);
    f32 tmax = max(tx1, tx2);
    f32 ty1 = (b.bound_min.y - ray.o.y) * ray.inv_nrm.y;
    f32 ty2 = (b.bound_max.y - ray.o.y) * ray.inv_nrm.y;
    tmin = max(tmin, min(ty1, ty2));
    tmax = min(tmax, max(ty1, ty2));
    f32 tz1 = (b.bound_min.z - ray.o.z) * ray.inv_nrm.z;
    f32 tz2 = (b.bound_max.z - ray.o.z) * ray.inv_nrm.z;
    tmin = max(tmin, min(tz1, tz2));
    tmax = min(tmax, max(tz1, tz2));

    result.hit = false;
    if (tmax >= tmin) {
        if (tmin > 0) {
            result.dist = tmin;
            result.hit = true;
        } else if (tmax > 0) {
            result.dist = tmax;
            result.hit = true;
            result.internal_fac = -1.0;
            tmin = tmax;
        }
    }

    b32 is_x = tmin == tx1 || tmin == tx2;
    b32 is_y = tmin == ty1 || tmin == ty2;
    b32 is_z = tmin == tz1 || tmin == tz2;

    if (is_z) {
        if (ray.nrm.z < 0) {
            result.nrm = f32vec3(0, 0, 1);
        } else {
            result.nrm = f32vec3(0, 0, -1);
        }
    } else if (is_y) {
        if (ray.nrm.y < 0) {
            result.nrm = f32vec3(0, 1, 0);
        } else {
            result.nrm = f32vec3(0, -1, 0);
        }
    } else {
        if (ray.nrm.x < 0) {
            result.nrm = f32vec3(1, 0, 0);
        } else {
            result.nrm = f32vec3(-1, 0, 0);
        }
    }
    result.nrm *= result.internal_fac;

    return result;
}
IntersectionRecord intersect(Ray ray, Capsule cap) {
    IntersectionRecord result;
    default_init(result);

    f32vec3 ba = cap.p1 - cap.p0;
    f32vec3 oa = ray.o - cap.p0;
    f32 baba = dot(ba, ba);
    f32 bard = dot(ba, ray.nrm);
    f32 baoa = dot(ba, oa);
    f32 rdoa = dot(ray.nrm, oa);
    f32 oaoa = dot(oa, oa);
    f32 a = baba - bard * bard;
    f32 b = baba * rdoa - baoa * bard;
    f32 c = baba * oaoa - baoa * baoa - cap.r * cap.r * baba;
    f32 h = b * b - a * c;
    if (h >= 0.) {
        f32 t = (-b - sqrt(h)) / a;
        f32 d = MAX_DIST;
        f32 y = baoa + t * bard;
        if (y > 0. && y < baba) {
            d = t;
        } else {
            f32vec3 oc = (y <= 0.) ? oa : ray.o - cap.p1;
            b = dot(ray.nrm, oc);
            c = dot(oc, oc) - cap.r * cap.r;
            h = b * b - c;
            if (h > 0.0) {
                d = -b - sqrt(h);
            }
        }
        cap.p0 = ray.o + ray.nrm * d - cap.p0;
        f32 h = clamp(dot(cap.p0, ba) / dot(ba, ba), 0.0, 1.0);
        result.nrm = (cap.p0 - h * ba) / cap.r;
        result.dist = d;
        result.hit = d != MAX_DIST && d >= 0;
    }
    return result;
}
