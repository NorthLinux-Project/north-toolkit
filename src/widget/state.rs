#[derive(Debug, Clone, PartialEq)]
pub struct Property<T: Clone + PartialEq> {
    value: T,
    dirty: bool,
}

impl<T: Clone + PartialEq> Property<T> {
    pub fn new(initial: T) -> Self {
        Self {
            value: initial,
            dirty: false,
        }
    }

    pub fn get(&self) -> &T {
        &self.value
    }

    pub fn set(&mut self, new_value: T) -> bool {
        if self.value != new_value {
            self.value = new_value;
            self.dirty = true;
            true
        } else {
            false
        }
    }

    pub fn is_dirty(&self) -> bool {
        self.dirty
    }

    pub fn clear_dirty(&mut self) {
        self.dirty = false;
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_property_dirty_detection() {
        let mut prop = Property::new(42);
        assert!(!prop.is_dirty());

        let changed = prop.set(42);
        assert!(!changed);
        assert!(!prop.is_dirty());

        let changed2 = prop.set(100);
        assert!(changed2);
        assert!(prop.is_dirty());
        assert_eq!(*prop.get(), 100);

        prop.clear_dirty();
        assert!(!prop.is_dirty());
    }
}
