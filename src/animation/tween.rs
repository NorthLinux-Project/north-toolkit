pub trait Lerp: Copy + PartialEq {
    fn lerp(a: Self, b: Self, t: f32) -> Self;
}

impl Lerp for f32 {
    #[inline]
    fn lerp(a: Self, b: Self, t: f32) -> Self {
        a + (b - a) * t
    }
}

impl Lerp for [f32; 2] {
    #[inline]
    fn lerp(a: Self, b: Self, t: f32) -> Self {
        [a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t]
    }
}

impl Lerp for [f32; 4] {
    #[inline]
    fn lerp(a: Self, b: Self, t: f32) -> Self {
        [
            a[0] + (b[0] - a[0]) * t,
            a[1] + (b[1] - a[1]) * t,
            a[2] + (b[2] - a[2]) * t,
            a[3] + (b[3] - a[3]) * t,
        ]
    }
}

#[derive(Debug, Clone, Copy, PartialEq)]
pub enum Easing {
    Linear,
    EaseIn,
    EaseOut,
    EaseInOut,
    CubicBezier(f32, f32, f32, f32),
}

impl Easing {
    pub fn sample(&self, t: f32) -> f32 {
        let t = t.clamp(0.0, 1.0);
        match *self {
            Easing::Linear => t,
            Easing::EaseIn => t * t * t,
            Easing::EaseOut => {
                let p = 1.0 - t;
                1.0 - p * p * p
            }
            Easing::EaseInOut => {
                if t < 0.5 {
                    4.0 * t * t * t
                } else {
                    let p = -2.0 * t + 2.0;
                    1.0 - (p * p * p) / 2.0
                }
            }
            Easing::CubicBezier(x1, y1, x2, y2) => sample_cubic_bezier(x1, y1, x2, y2, t),
        }
    }
}

fn sample_cubic_bezier(x1: f32, y1: f32, x2: f32, y2: f32, target_x: f32) -> f32 {
    let mut t = target_x;
    for _ in 0..8 {
        let current_x = 3.0 * (1.0 - t) * (1.0 - t) * t * x1
            + 3.0 * (1.0 - t) * t * t * x2
            + t * t * t;
        let dx = 3.0 * (1.0 - t) * (1.0 - t) * x1
            + 6.0 * (1.0 - t) * t * (x2 - x1)
            + 3.0 * t * t * (1.0 - x2);
        if dx.abs() < 1e-6 {
            break;
        }
        let diff = current_x - target_x;
        t -= diff / dx;
        t = t.clamp(0.0, 1.0);
    }

    3.0 * (1.0 - t) * (1.0 - t) * t * y1
        + 3.0 * (1.0 - t) * t * t * y2
        + t * t * t
}

#[derive(Debug, Clone, Copy, PartialEq)]
pub struct Tween<T: Lerp> {
    pub from: T,
    pub to: T,
    pub current: T,
    pub duration_ms: f32,
    pub elapsed_ms: f32,
    pub easing: Easing,
}

impl<T: Lerp> Tween<T> {
    pub fn new(initial: T) -> Self {
        Self {
            from: initial,
            to: initial,
            current: initial,
            duration_ms: 0.0,
            elapsed_ms: 0.0,
            easing: Easing::EaseOut,
        }
    }

    pub fn set_target(&mut self, target: T, duration_ms: f32, easing: Easing) {
        if self.to == target && !self.is_complete() {
            return;
        }
        self.from = self.current;
        self.to = target;
        self.duration_ms = duration_ms.max(0.001);
        self.elapsed_ms = 0.0;
        self.easing = easing;
    }

    pub fn tick(&mut self, delta_ms: f32) -> T {
        if self.elapsed_ms < self.duration_ms {
            self.elapsed_ms = (self.elapsed_ms + delta_ms).min(self.duration_ms);
            let progress = self.elapsed_ms / self.duration_ms;
            let factor = self.easing.sample(progress);
            self.current = T::lerp(self.from, self.to, factor);
        } else {
            self.current = self.to;
        }
        self.current
    }

    pub fn is_complete(&self) -> bool {
        self.elapsed_ms >= self.duration_ms
    }

    pub fn value(&self) -> T {
        self.current
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_lerp_f32() {
        assert_eq!(f32::lerp(10.0, 20.0, 0.5), 15.0);
        assert_eq!(f32::lerp(0.0, 100.0, 1.0), 100.0);
        assert_eq!(f32::lerp(50.0, 100.0, 0.0), 50.0);
    }

    #[test]
    fn test_easing_bounds() {
        for easing in [
            Easing::Linear,
            Easing::EaseIn,
            Easing::EaseOut,
            Easing::EaseInOut,
            Easing::CubicBezier(0.25, 0.1, 0.25, 1.0),
        ] {
            assert!((easing.sample(0.0) - 0.0).abs() < 1e-4);
            assert!((easing.sample(1.0) - 1.0).abs() < 1e-4);
        }
    }

    #[test]
    fn test_tween_progression() {
        let mut tween = Tween::new(0.0f32);
        tween.set_target(100.0, 200.0, Easing::Linear);
        assert!(!tween.is_complete());

        tween.tick(100.0);
        assert!((tween.value() - 50.0).abs() < 1e-3);
        assert!(!tween.is_complete());

        tween.tick(100.0);
        assert!((tween.value() - 100.0).abs() < 1e-3);
        assert!(tween.is_complete());
    }
}
