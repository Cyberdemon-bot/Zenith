#[derive(Debug, Clone, Copy, PartialEq)]
pub enum Object
{
    Integer(i32),
    Float(f32)
}

pub struct VirtualMachine
{
    pub instructions: Vec<u8>,
    pub constants: Vec<Object>,
    pub stack: Vec<Object>,
    pub ip: usize
}

impl VirtualMachine
{
    pub fn new(instructions: Vec<u8>, constants: Vec<Object>) -> Self
    {
        VirtualMachine
        {
            instructions,
            constants,
            stack: Vec::with_capacity(2048),
            ip: 0
        }
    }

    fn push(&mut self, obj: Object)
    {
        self.stack.push(obj);
    }

    fn pop(&mut self) -> Object
    {
        self.stack.pop().expect("Stack is empty")
    }

    pub fn stack_top(&self) -> Option<&Object> 
    {
        self.stack.last()
    }

    pub fn run(&mut self)
    {
        while self.ip < self.instructions.len()
        {
            let opcode = self.instructions[self.ip]; 
            self.ip += 1;
            match opcode
            {
                0 => {
                    let raw_bytes = [
                        self.instructions[self.ip],
                        self.instructions[self.ip + 1]
                    ];
                    self.ip += 2;
                    let const_index = u16::from_be_bytes(raw_bytes) as usize;
                    let constant_value = self.constants[const_index];
                    self.push(constant_value);
                }

                1 => {
                    let right = self.pop();
                    let left = self.pop();

                    match (left, right) 
                    {
                        (Object::Integer(l), Object::Integer(r)) => self.push(Object::Integer(l + r)),
                        (Object::Float(l), Object::Float(r)) => self.push(Object::Float(l + r)),
                        _ => panic!("Type error when add"),
                    }
                }

                2 => { 
                    let right = self.pop();
                    let left = self.pop();

                    match (left, right) {
                        (Object::Integer(l), Object::Integer(r)) => self.push(Object::Integer(l - r)),
                        (Object::Float(l), Object::Float(r)) => self.push(Object::Float(l - r)),
                        _ => panic!("Type error when sub!"),
                    }
                }

                3 => { 
                    self.pop();
                }

                _ => panic!("Unknown opcode: {}", opcode)
            }
            ;
        }
    }
}


#[cfg(test)]
mod tests 
{
    use super::*;

    #[test]
    fn test_simple_bytecode_execution() 
    {
        let constants = vec![Object::Integer(10), Object::Integer(3)];
        let bytecode = vec![
            0, 0, 0, 
            0, 0, 1, 
            2,     
        ];

        let mut vm = VirtualMachine::new(bytecode, constants);
        vm.run();

        assert_eq!(vm.stack_top(), Some(&Object::Integer(7)));
        println!("Ans {:?}", vm.stack_top().unwrap());
    }
}