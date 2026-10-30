//! Reactive property container with fine-grained dirty bit tracking for relayout and repaint.

/// A reactive value container that tracks whether its value has mutated.
#[derive(Debug, Clone, PartialEq)]
pub struct Property<T: Clone + PartialEq> {
    value: T,
    dirty: bool,
}

impl<T: Clone + PartialEq> Property<T> {
    /// Creates a new property initialized to `initial` with dirty flag set to `false`.
    pub fn new(initial: T) -> Self {
        Self {
            value: initial,
            dirty: false,
        }
    }

    /// Borrows the current value.
    pub fn get(&self) -> &T {
        &self.value
    }

    /// Updates the value and sets the dirty flag to `true` if `new_value != self.value`.
    /// Returns `true` if the value changed.
    pub fn set(&mut self, new_value: T) -> bool {
        if self.value != new_value {
            self.value = new_value;
            self.dirty = true;
            true
        } else {
            false
        }
    }

    /// Returns `true` if the property has been mutated since last `clear_dirty`.
    pub fn is_dirty(&self) -> bool {
        self.dirty
    }

    /// Resets the dirty flag to `false`.
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
